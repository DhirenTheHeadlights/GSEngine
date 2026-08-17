export module gse.runtime:bootstrap;

import std;

import gse.core;
import gse.math;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.introspection;
import gse.os;
import gse.gpu;
import gse.log;
import gse.stacktrace;
import gse.win32;
import gse.config;
import gse.scenario;

import :attached_link;
import :bench;
import :engine;
import :frame_pacing;
import :world_system;

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
		std::abort();
	});

	should_shutdown.store(false, std::memory_order_relaxed);

	if (config.bench.enabled) {
		begin_bench(config.bench);
	}

	engine e(config);
	log::println(log::level::info, "Starting GSEngine...");

	task::start([&] {
		e.initialize(setup);
		trace::finalize_frame();

		watchdog::start();

		log::set_level(log::level::info);
		log::enable_backtrace(256);
		log::set_async(true);

		win32::HANDLE editor_pipe = nullptr;
		bool surface_announced = false;
		bool graph_dumped = false;
		attached_pipe_reader pipe_reader{};
		frame_pacing pacing{};
		bench_state bench{};
		if (!config.bench.enabled) {
			profile::set_frame_recording(config.attached);
		}

		if (config.attached && !config.ipc_pipe_name.empty()) {
			const std::wstring pipe(config.ipc_pipe_name.begin(), config.ipc_pipe_name.end());
			editor_pipe = win32::CreateFileW(pipe.c_str(), win32::generic_write | win32::generic_read, 0, nullptr, win32::open_existing, 0, nullptr);
			if (win32::valid_handle(editor_pipe)) {
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
		const auto sync_begin_id = trace_id<"frame::sync_begin">();
		const auto sync_end_id = trace_id<"frame::sync_end">();
		const auto finalize_id = trace_id<"frame::finalize_trace">();
		const auto ingest_id = trace_id<"frame::ingest_profile">();
		const auto update_id = trace_id<"engine::update">();
		const auto render_id = trace_id<"engine::render">();

		while (!should_shutdown.load(std::memory_order_acquire)) {
			{
				trace::scope_guard sg{ loop_id };
				watchdog::section watch{ loop_id, seconds(5.f) };
				if (e.all_settled()) {
					trace::scope_guard sg{ pace_id };
					pace_frame(pacing);
				}
				if (config.create_window) {
					{
						trace::scope_guard sg{ poll_id };
						e.tick_window();
					}

					if (e.window_should_close()) {
						shutdown();
					}
				}

				if (editor_pipe) {
					drain_editor_pipe(editor_pipe, pipe_reader, e, pacing);
				}

				{
					trace::scope_guard sg{ sync_begin_id };
					frame_sync::begin();
				}

				{
					trace::scope_guard sg{ e.id() };
					const auto update_begin = system_clock::now<time_t<double, seconds>>();
					{
						trace::scope_guard sg{ update_id };
						watchdog::section watch{ update_id, seconds(3.f) };
						e.update();
					}
					const auto update_end = system_clock::now<time_t<double, seconds>>();

					if (config.render) {
						{
							trace::scope_guard sg{ render_id };
							watchdog::section watch{ render_id, seconds(3.f) };
							e.render();
						}
					}

					if (const auto render_end = system_clock::now<time_t<double, seconds>>(); render_end - update_begin > milliseconds(100.0)) {
						log::println(
							log::level::warning,
							log::category::general,
							"slow frame: update={:.1f:ms} render={:.1f:ms}",
							time_t<double, seconds>(update_end - update_begin),
							time_t<double, seconds>(render_end - update_end)
						);
					}
				}

				if (editor_pipe && !surface_announced && e.attached_surface_ready()) {
					attached_surface_message msg = e.attached_message();
					msg.pid = win32::GetCurrentProcessId();
					win32::DWORD written = 0;
					win32::WriteFile(editor_pipe, &msg, sizeof(msg), &written, nullptr);
					surface_announced = true;
					log::println(log::category::general, "announced shared surface ring to editor: {}x{}", msg.extent.x(), msg.extent.y());
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
					trace::scope_guard sg{ sync_end_id };
					frame_sync::end();
				}
			}

			{
				trace::scope_guard sg{ finalize_id };
				trace::finalize_frame();
			}
			{
				trace::scope_guard sg{ ingest_id };
				profile::ingest_frame();
			}

			if (config.bench.enabled) {
				if (step_bench(config.bench, bench, e)) {
					shutdown();
				}
			}
		}

		if (pacing.timer) {
			win32::CloseHandle(pacing.timer);
		}
	});

	const time exit_budget = seconds(10.f);

	{
		watchdog::section watch{ trace_id<"exit::log_drain">(), exit_budget };
		log::set_async(false);
	}

	{
		watchdog::section watch{ trace_id<"exit::engine_shutdown">(), exit_budget };
		e.shutdown();
	}

	{
		watchdog::section watch{ trace_id<"exit::relaunch">(), exit_budget };
		app::run_pending_relaunch();
	}

	watchdog::stop();
}