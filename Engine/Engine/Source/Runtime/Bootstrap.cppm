export module gse.runtime:bootstrap;

import gse.assert;
import gse.concurrency;
import gse.config;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.introspection;
import gse.log;
import gse.math;
import gse.network;
import gse.os;
import gse.scenario;
import gse.stacktrace;
import gse.time;
import gse.win32;
import std;

import :attached_link;
import :bench;
import :engine;
import :frame_pacing;

export namespace gse {
	using app_setup_fn = std::function<void(engine&)>;

	auto start(
		app_setup_fn setup,
		const engine_config& config = {}
	) -> void;

	auto shutdown() -> void;
}

namespace gse {
	std::atomic<bool> should_shutdown = false;
}

auto gse::shutdown() -> void {
	should_shutdown.store(true, std::memory_order_release);
}

auto gse::start(app_setup_fn setup, const engine_config& config) -> void {
	install_crash_handlers();

	std::set_terminate([] {
		const auto stack = capture_stacktrace(1);
		if (const auto ex = std::current_exception()) {
			try {
				std::rethrow_exception(ex);
			}
			catch (const std::exception& e) {
				log::println(
					log::level::error,
					log::category::general,
					"std::terminate called via uncaught exception (type={}): {}\nStack:\n{}",
					typeid(e).name(),
					e.what(),
					stack
				);
			}
			catch (...) {
				log::println(
					log::level::error,
					log::category::general,
					"std::terminate called via uncaught unknown exception\nStack:\n{}",
					stack
				);
			}
		}
		else {
			log::println(
				log::level::error,
				log::category::general,
				"std::terminate called with no current exception\nStack:\n{}",
				stack
			);
		}
		log::flush();
		fatal_exit(3);
	});

	should_shutdown.store(false, std::memory_order_relaxed);

	if (config.bench.enabled) {
		begin_bench(config.bench);
	}

	engine e(config);
	log::println(log::level::info, "Starting GSEngine...");

	const std::size_t worker_threads = config.worker_threads > 0
		? std::max<std::size_t>(2, config.worker_threads)
		: std::thread::hardware_concurrency();
	const time exit_budget = seconds(10.f);

	task::start([&] {
		e.initialize(setup);
		trace::finalize_frame();
		trace::set_enabled(config.trace);

		watchdog::start();

		log::set_level(log::level::info);
		log::enable_backtrace(256);
		log::set_async(true);

		win32::HANDLE editor_pipe = nullptr;
		std::uint32_t announced_revision = 0;
		bool graph_dumped = false;
		attached_pipe_reader pipe_reader{};
		frame_pacing pacing{};
		bench_state bench{};
		const bool dedicated = !config.create_window
			&& !config.bench.enabled
			&& network::resolve_role(config.net) == network::session_role::dedicated;
		if (!config.bench.enabled) {
			profile::set_frame_recording(config.attached);
		}
		if (dedicated && config.net.dashboard) {
			log::set_console_output(false);
		}

		if (config.attached && !config.ipc_pipe_name.empty()) {
			const std::wstring pipe(config.ipc_pipe_name.begin(), config.ipc_pipe_name.end());
			editor_pipe = win32::CreateFileW(pipe.c_str(), win32::generic_write | win32::generic_read, 0, nullptr, win32::open_existing, 0, nullptr);
			if (win32::valid_handle(editor_pipe)) {
				set_fatal_pipe(editor_pipe);
				install_fatal_reporter(&report_fatal_to_editor);
				log::println(log::level::info, log::category::general, "attached to editor via {} (pid {})", config.ipc_pipe_name, win32::GetCurrentProcessId());
			}
			else {
				editor_pipe = nullptr;
				e.abandon_attach();
				log::println(log::level::warning, log::category::general, "attached mode: could not open editor pipe {} (error {}); falling back to detached", config.ipc_pipe_name, win32::GetLastError());
			}
		}

		const auto loop_id = trace_id<"frame::loop">();
		const auto pace_id = trace_id<"frame::pace">();
		const auto poll_id = trace_id<"frame::poll_events">();
		const auto wait_id = trace_id<"frame::wait_events">();
		const auto sync_begin_id = trace_id<"frame::sync_begin">();
		const auto sync_end_id = trace_id<"frame::sync_end">();
		const auto finalize_id = trace_id<"frame::finalize_trace">();
		const auto ingest_id = trace_id<"frame::ingest_profile">();
		const auto update_id = trace_id<"engine::update">();
		const auto render_id = trace_id<"engine::render">();

		const bool reactive = config.create_window && config.cadence == loop_cadence::reactive;
		const auto idle_floor = seconds(1.f);

		const auto slow_frame_window = seconds(5.f);
		std::size_t slow_frames = 0;
		time_t<double, seconds> slow_frame_worst{};
		time_t<double, seconds> slow_frame_worst_update{};
		time_t<double, seconds> slow_frame_worst_render{};
		interval_timer<> slow_frame_report{ slow_frame_window };

		if (reactive) {
			frame_demand::install_waker(window::post_wake);
		}

		while (!should_shutdown.load(std::memory_order_acquire)) {
			{
				trace::scope_guard _{ loop_id };
				watchdog::section _{ loop_id, seconds(5.f) };
				if (dedicated && e.all_settled()) {
					trace::scope_guard _{ pace_id };
					pace_dedicated(pacing);
				}
				if (config.create_window) {
					if (reactive && e.all_settled() && !frame_demand::active()) {
						trace::scope_guard _{ wait_id };
						window::wait_events(frame_demand::wait_budget(idle_floor));
					}

					{
						trace::scope_guard _{ poll_id };
						e.tick_window();
					}

					if (e.window_should_close()) {
						shutdown();
					}

					frame_demand::consume_redraw();
				}

				if (editor_pipe) {
					drain_editor_pipe(editor_pipe, pipe_reader, e);
				}

				{
					trace::scope_guard _{ sync_begin_id };
					frame_sync::begin();
				}

				{
					trace::scope_guard _{ e.id() };
					const auto update_begin = system_clock::now<time_t<double, seconds>>();
					{
						trace::scope_guard _{ update_id };
						watchdog::section _{ update_id, seconds(3.f) };
						e.update();
					}
					const auto update_end = system_clock::now<time_t<double, seconds>>();

					if (config.render) {
						{
							trace::scope_guard _{ render_id };
							watchdog::section _{ render_id, seconds(3.f) };
							e.render();
						}
					}

					if (const auto render_end = system_clock::now<time_t<double, seconds>>(); render_end - update_begin > milliseconds(100.0)) {
						++slow_frames;
						if (const auto frame_time = time_t<double, seconds>(render_end - update_begin); frame_time > slow_frame_worst) {
							slow_frame_worst = frame_time;
							slow_frame_worst_update = time_t<double, seconds>(update_end - update_begin);
							slow_frame_worst_render = time_t<double, seconds>(render_end - update_end);
						}
					}
				}

				if (slow_frame_report.tick() && slow_frames != 0) {
					log::println(
						log::level::warning,
						log::category::general,
						"{} slow frames in the last {:.0f:s}, worst update={:.1f:ms} render={:.1f:ms}",
						slow_frames,
						slow_frame_window,
						slow_frame_worst_update,
						slow_frame_worst_render
					);
					slow_frames = 0;
					slow_frame_worst = {};
					slow_frame_worst_update = {};
					slow_frame_worst_render = {};
				}

				if (editor_pipe && e.attached_surface_ready() && e.attached_message().revision != announced_revision) {
					attached_surface_message msg = e.attached_message();
					msg.pid = win32::GetCurrentProcessId();
					win32::DWORD written = 0;
					win32::WriteFile(editor_pipe, &msg, sizeof(msg), &written, nullptr);
					announced_revision = msg.revision;
					log::println(log::category::general, "announced shared surface ring to editor: {}x{} (revision {})", msg.extent.x(), msg.extent.y(), msg.revision);
				}

				if (!graph_dumped && !config.dump_system_graph_path.empty() && e.all_settled()) {
					const introspection::system_graph graph = e.snapshot_graph();
					std::ofstream graph_out(config.dump_system_graph_path, std::ios::binary);
					if (graph_out) {
						binary_writer writer(graph_out, introspection::system_graph_magic, introspection::system_graph_version);
						writer & graph.nodes;
						writer & graph.edges;
					}
					graph_dumped = true;
					log::println(log::category::general, "wrote system graph: {} nodes to {}", graph.nodes.size(), config.dump_system_graph_path);
				}

				{
					trace::scope_guard _{ sync_end_id };
					frame_sync::end();
				}
			}

			{
				trace::scope_guard _{ finalize_id };
				trace::finalize_frame();
			}
			{
				trace::scope_guard _{ ingest_id };
				profile::ingest_frame();
			}

			if (config.bench.enabled) {
				if (step_bench(config.bench, bench, e)) {
					shutdown();
				}
			}
		}

		frame_demand::clear_waker();

		if (pacing.timer) {
			win32::CloseHandle(pacing.timer);
		}

		{
			watchdog::section _{ trace_id<"exit::task_drain">(), exit_budget };
			task::request_shutdown();
			if (!task::wait_idle_for(exit_budget)) {
				log::println(log::level::error, log::category::task, "exit: jobs still in flight after {::s}; shutting systems down anyway", exit_budget);
			}
		}

		{
			watchdog::section _{ trace_id<"exit::engine_shutdown">(), exit_budget };
			e.shutdown();
		}
	}, worker_threads);

	{
		watchdog::section _{ trace_id<"exit::log_drain">(), exit_budget };
		log::set_async(false);
	}

	{
		watchdog::section _{ trace_id<"exit::relaunch">(), exit_budget };
		app::run_pending_relaunch();
	}

	watchdog::stop();
}