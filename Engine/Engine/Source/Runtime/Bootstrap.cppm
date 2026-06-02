export module gse.runtime:bootstrap;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.gpu;
import gse.log;
import gse.stacktrace;

import :engine;

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

	engine e(config);
	log::println(log::level::info, "Starting GSEngine...");

	task::start([&] {
		e.initialize(setup);
		trace::finalize_frame();

		const auto loop_id = trace_id<"frame::loop">();
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
				if (config.create_window) {
					{
						trace::scope_guard sg{ poll_id };
						window::poll_events();
					}

					e.pump_window();

					if (e.window_should_close()) {
						gse::shutdown();
					}
				}

				{
					trace::scope_guard sg{ sync_begin_id };
					frame_sync::begin();
				}

				{
					trace::scope_guard sg{ e.id() };
					{
						trace::scope_guard sg{ update_id };
						e.update();
					}

					if (config.render) {
						{
							trace::scope_guard sg{ render_id };
							e.render();
						}
					}
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
		}

		task::wait_idle();
	});

	e.shutdown();
}
