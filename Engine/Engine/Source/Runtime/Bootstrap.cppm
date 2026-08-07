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

import :engine;
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

	enum class bench_phase : std::uint8_t {
		settling,
		warmup,
		measuring,
		done,
	};

	constexpr int bench_settle_frame_limit = 6000;

	struct bench_state {
		bench_phase phase = bench_phase::settling;
		int frames_in_phase = 0;
		bool scene_requested = false;
		bool scenario_started = false;
		scenario::wait_gate gate;
		std::optional<scenario::context> scenario_ctx;
		async::task<> scenario_task;
	};

	constexpr std::uint32_t pacing_window = 120;
	constexpr int pacing_max_divisor = 4;

	struct frame_pacing {
		win32::HANDLE timer = nullptr;
		bool timer_created = false;
		time_t<std::uint64_t> refresh{};
		int divisor = 1;
		time_t<double, seconds> next_deadline{};
		bool deadline_valid = false;
		time_t<double, seconds> last_entry{};
		time_t<double, seconds> last_wait{};
		time_t<double, seconds> ema_busy{};
		time_t<double, seconds> next_heartbeat{};
		time_t<double, seconds> allow_down_at{};
		time_t<double, seconds> last_down_at{};
		time_t<double, seconds> down_backoff{};
		std::uint32_t window_frames = 0;
		std::uint32_t window_misses = 0;
		int pending_divisor = 1;
	};

	struct attached_pipe_reader {
		std::array<char, std::max(sizeof(attached_input_message), sizeof(attached_pacing_message))> bytes{};
		std::size_t received = 0;
		std::size_t expected = sizeof(std::uint32_t);
		bool have_magic = false;
	};

	auto pace_wait(
		frame_pacing& pacing,
		time_t<double, seconds> deadline
	) -> void;

	auto pace_frame(
		frame_pacing& pacing
	) -> void;

	auto drain_editor_pipe(
		win32::HANDLE& editor_pipe,
		attached_pipe_reader& reader,
		engine& e,
		frame_pacing& pacing
	) -> void;

	auto bench_world_ready(
		const bench_config& config,
		engine& e
	) -> bool;

	auto drive_scenario(
		const bench_config& config,
		bench_state& state,
		engine& e
	) -> void;

	auto begin_bench(
		const bench_config& config
	) -> void;

	auto step_bench(
		const bench_config& config,
		bench_state& state,
		engine& e
	) -> void;

	auto finish_bench(
		const bench_config& config,
		const bench_state& state,
		engine& e
	) -> void;
}

auto gse::bench_world_ready(const bench_config& config, engine& e) -> bool {
	return e.all_settled() && (config.scene.empty() || e.world_populated());
}

auto gse::drive_scenario(const bench_config& config, bench_state& state, engine& e) -> void {
	if (!config.scenario_body) {
		return;
	}

	if (!state.scenario_started) {
		state.scenario_started = true;
		state.scenario_ctx.emplace(e.make_channel_writer(), &state.gate);
		state.scenario_task = config.scenario_body(*state.scenario_ctx);
		if (!async::resume_checked(async::track_frame(state.scenario_task.consume_start_handle()))) {
			log::println(log::level::error, log::category::general, "bench: the scenario coroutine refused its first resume; this run is unscripted");
		}
		return;
	}

	state.scenario_ctx->advance_frame();

	if (!state.gate.armed) {
		return;
	}

	if (state.gate.until_settled) {
		if (!bench_world_ready(config, e)) {
			return;
		}
	}
	else {
		if (state.gate.frames_remaining > 0) {
			--state.gate.frames_remaining;
		}
		if (state.gate.frames_remaining > 0) {
			return;
		}
	}

	state.gate.armed = false;
	if (!async::resume_checked(state.gate.resume)) {
		log::println(log::level::error, log::category::general, "bench: the scenario coroutine frame went stale; the rest of the scripted workload will not run");
	}
}

auto gse::begin_bench(const bench_config& config) -> void {
	system_clock::set_fixed_step_override(1);
	profile::set_enabled(true);
	profile::set_frame_recording(true);
	profile::set_recording_capacity(static_cast<std::size_t>(std::max(config.frames, 1)));

	log::println(
		log::category::general,
		"bench: fixed-step clock engaged, {} warmup frames then {} measured frames",
		config.warmup_frames,
		config.frames
	);

	if (!config.scenario.empty() && !config.scenario_body) {
		log::println(
			log::level::error,
			log::category::general,
			"bench: scenario '{}' was named but no body was resolved for it; the application owns that lookup, so this run measures an unscripted world",
			config.scenario
		);
	}
}

auto gse::step_bench(const bench_config& config, bench_state& state, engine& e) -> void {
	drive_scenario(config, state, e);

	switch (state.phase) {
		case bench_phase::settling: {
			if (!state.scene_requested) {
				state.scene_requested = true;
				if (!config.scene.empty()) {
					activate_scene(e.world(), find_or_generate_id(config.scene));
					log::println(log::category::general, "bench: requested scene '{}'", config.scene);
				}
			}

			if (bench_world_ready(config, e)) {
				state.phase = bench_phase::warmup;
				state.frames_in_phase = 0;
				log::println(log::category::general, "bench: world settled, warming up");
				break;
			}

			if (++state.frames_in_phase >= bench_settle_frame_limit) {
				log::println(
					log::level::error,
					log::category::general,
					"bench: world never became ready within {} frames (scene '{}' populated={}); aborting without a measurement",
					bench_settle_frame_limit,
					config.scene,
					e.world_populated()
				);
				state.phase = bench_phase::done;
				gse::shutdown();
			}
			break;
		}
		case bench_phase::warmup: {
			if (++state.frames_in_phase >= config.warmup_frames) {
				profile::reset();
				state.phase = bench_phase::measuring;
				state.frames_in_phase = 0;
				log::println(log::category::general, "bench: warmup complete, measuring");
			}
			break;
		}
		case bench_phase::measuring: {
			if (++state.frames_in_phase >= config.frames) {
				state.phase = bench_phase::done;
				finish_bench(config, state, e);
				gse::shutdown();
			}
			break;
		}
		case bench_phase::done: {
			break;
		}
	}
}

auto gse::finish_bench(const bench_config& config, const bench_state& state, engine& e) -> void {
	if (config.scenario_body && !state.scenario_task.done()) {
		log::println(
			log::level::warning,
			log::category::general,
			"bench: the frame budget expired while the scenario was still running, so part of the scripted workload never happened"
		);
	}

	const std::string stem = config.scenario.empty()
		? std::format("{}_bench", config::executable_stem())
		: config.scenario;
	const std::filesystem::path profile_path = config.profile_out.empty()
		? config::profile_dir() / std::format("{}.gsprof", stem)
		: std::filesystem::path(config.profile_out);
	const std::filesystem::path summary_path = config.summary_out.empty()
		? config::profile_dir() / std::format("{}.txt", stem)
		: std::filesystem::path(config.summary_out);

	const profile::report_file file = profile::build_report_file();
	const profile::run_summary summary = profile::summarize(file);
	profile::write_summary(summary, summary_path);
	profile::dump_report(profile_path);

	log::println(
		log::category::general,
		"bench: {} frames measured, p50 {:.3f:ms} p95 {:.3f:ms} p99 {:.3f:ms}, world-state hash {:#018x}",
		summary.frames,
		summary.frame_p50,
		summary.frame_p95,
		summary.frame_p99,
		e.world_state_hash()
	);
	log::println(log::category::general, "bench: wrote {} and {}", profile_path.display_string(), summary_path.display_string());
}

auto gse::shutdown() -> void {
	should_shutdown.store(true, std::memory_order_release);
}

auto gse::pace_wait(frame_pacing& pacing, const time_t<double, seconds> deadline) -> void {
	if (!pacing.timer_created) {
		pacing.timer_created = true;
		pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, win32::create_waitable_timer_high_resolution, win32::timer_all_access);
		if (!pacing.timer) {
			pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, 0, win32::timer_all_access);
		}
	}

	const time_t<double, seconds> spin_margin = microseconds(400.0);
	const auto begin = system_clock::now<time_t<double, seconds>>();
	if (pacing.timer && deadline - begin > spin_margin) {
		const auto wait = deadline - begin - spin_margin;
		const win32::LARGE_INTEGER due{ .QuadPart = -static_cast<win32::LONGLONG>(wait / nanoseconds(100.0)) };
		if (win32::SetWaitableTimer(pacing.timer, &due, 0, nullptr, nullptr, 0)) {
			win32::WaitForSingleObject(pacing.timer, win32::infinite);
		}
	}
	while (system_clock::now<time_t<double, seconds>>() < deadline) {
		std::this_thread::yield();
	}
	pacing.last_wait = system_clock::now<time_t<double, seconds>>() - begin;
}

auto gse::pace_frame(frame_pacing& pacing) -> void {
	if (pacing.refresh == time_t<std::uint64_t>{}) {
		return;
	}

	const auto refresh = quantity_cast<time_t<double, seconds>>(time(pacing.refresh));
	system_clock::submit_refresh_interval(quantity_cast<system_clock::internal_time>(refresh));

	const auto target = refresh * static_cast<double>(pacing.divisor);
	const auto now = system_clock::now<time_t<double, seconds>>();

	if (!pacing.deadline_valid) {
		pacing.deadline_valid = true;
		pacing.next_deadline = now + target;
		pacing.last_entry = now;
		pacing.last_wait = {};
		pacing.ema_busy = target;
		pacing.next_heartbeat = now + seconds(10.0);
		pacing.allow_down_at = now;
		pacing.last_down_at = {};
		pacing.down_backoff = seconds(5.0);
		log::println(log::category::general, "attached pacing: engaged at editor refresh {:.3f:ms}", refresh);
		return;
	}

	const double busy_alpha = 0.1;
	const auto busy = now - pacing.last_entry - pacing.last_wait;
	pacing.last_entry = now;
	pacing.last_wait = {};
	pacing.ema_busy = pacing.ema_busy * (1.0 - busy_alpha) + busy * busy_alpha;

	++pacing.window_frames;
	if (now > pacing.next_deadline) {
		++pacing.window_misses;
		pacing.next_deadline = now + target;
		const auto miss_delta = busy > pacing.ema_busy * 2.0 ? time_t<double, seconds>(busy) : pacing.ema_busy;
		system_clock::submit_display_interval(quantity_cast<system_clock::internal_time>(miss_delta));
	}
	else {
		pace_wait(pacing, pacing.next_deadline);
		pacing.next_deadline += target;
		system_clock::submit_display_interval(quantity_cast<system_clock::internal_time>(target));
	}

	if (pacing.window_frames >= pacing_window) {
		const double load = pacing.ema_busy * 1.1 / refresh;
		auto desired = std::clamp(static_cast<int>(std::ceil(load)), 1, pacing_max_divisor);
		if (pacing.window_misses * 4 > pacing.window_frames) {
			desired = std::clamp(std::max(desired, pacing.divisor + 1), 1, pacing_max_divisor);
		}
		if (desired != pacing.divisor) {
			if (desired != pacing.pending_divisor) {
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms} suggests every {} refresh(es), confirming next window", pacing.ema_busy, desired);
			}
			else if (desired > pacing.divisor) {
				pacing.divisor = desired;
				if (now - pacing.last_down_at < seconds(6.0)) {
					if (const auto doubled = time_t<double, seconds>(pacing.down_backoff * 2.0); doubled < seconds(60.0)) {
						pacing.down_backoff = doubled;
					}
					else {
						pacing.down_backoff = seconds(60.0);
					}
				}
				pacing.allow_down_at = now + pacing.down_backoff;
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms}, now pacing to every {} editor refresh(es) ({:.3f:ms} target)", pacing.ema_busy, desired, refresh * static_cast<double>(desired));
			}
			else if (now >= pacing.allow_down_at) {
				pacing.divisor = desired;
				pacing.last_down_at = now;
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms}, probing every {} editor refresh(es) ({:.3f:ms} target)", pacing.ema_busy, desired, refresh * static_cast<double>(desired));
			}
		}
		pacing.pending_divisor = desired;
		pacing.window_frames = 0;
		pacing.window_misses = 0;
	}

	if (now >= pacing.next_heartbeat) {
		pacing.next_heartbeat = now + seconds(30.0);
		log::println(log::category::general, "attached pacing: divisor={} frame cost {:.3f:ms} misses={}/{} in current window", pacing.divisor, pacing.ema_busy, pacing.window_misses, pacing.window_frames);
	}
}

auto gse::drain_editor_pipe(win32::HANDLE& editor_pipe, attached_pipe_reader& reader, engine& e, frame_pacing& pacing) -> void {
	win32::DWORD available = 0;
	while (win32::PeekNamedPipe(editor_pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
		win32::DWORD read = 0;
		const auto remaining = static_cast<win32::DWORD>(reader.expected - reader.received);
		if (!win32::ReadFile(editor_pipe, reader.bytes.data() + reader.received, remaining, &read, nullptr) || read == 0) {
			break;
		}
		reader.received += read;
		if (reader.received < reader.expected) {
			continue;
		}

		if (!reader.have_magic) {
			std::uint32_t magic = 0;
			std::memcpy(&magic, reader.bytes.data(), sizeof(magic));
			std::size_t total = 0;
			if (magic == attached_input_magic) {
				total = sizeof(attached_input_message);
			}
			else if (magic == attached_pacing_magic) {
				total = sizeof(attached_pacing_message);
			}
			if (total == 0) {
				log::println(log::level::warning, log::category::general, "attached pipe: unknown message magic {:#x}; closing pipe", magic);
				win32::CloseHandle(editor_pipe);
				editor_pipe = nullptr;
				return;
			}
			reader.have_magic = true;
			reader.expected = total;
			continue;
		}

		std::uint32_t magic = 0;
		std::memcpy(&magic, reader.bytes.data(), sizeof(magic));
		if (magic == attached_input_magic) {
			attached_input_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			e.push_attached_input(message.event);
		}
		else {
			attached_pacing_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			pacing.refresh = message.refresh;
		}
		reader.received = 0;
		reader.expected = sizeof(std::uint32_t);
		reader.have_magic = false;
	}
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
						gse::shutdown();
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
				step_bench(config.bench, bench, e);
			}
		}

		watchdog::stop();

		if (pacing.timer) {
			win32::CloseHandle(pacing.timer);
		}

		task::wait_idle();
	});

	log::set_async(false);
	e.shutdown();

	app::run_pending_relaunch();
}
