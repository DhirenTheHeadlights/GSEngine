export module gse.runtime:engine;

import std;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.physics;
import gse.network;

import :world;

export namespace gse {
	struct engine : hookable<engine> {
		explicit engine(const std::string& name) : hookable(name) {}

		world world;
	};

	struct engine_config {
		std::string title = "GSEngine Application";
		std::optional<unitless::vec2> size = std::nullopt;
		bool resizable = true;
		bool fullscreen = false;
	};

	enum struct flags : std::uint8_t {
		none = 0,
		create_window = 1 << 0,
		render = 1 << 1,
	};

	constexpr auto operator|(
		flags lhs,
		flags rhs
	) -> flags;

	constexpr auto has_flag(
		flags haystack,
		flags needle
	) -> bool;
}

namespace gse {
	auto initialize(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	auto update(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	auto render(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	engine engine("GSEngine");
	std::atomic should_shutdown = false;
}

export namespace gse {
	template <typename... Args>
	auto start(
		flags engine_flags = flags::create_window | flags::render,
		const engine_config& config = {}
	) -> void;

	auto shutdown(
	) -> void;
}

constexpr auto gse::operator|(flags lhs, flags rhs) -> flags {
	return static_cast<flags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr auto gse::has_flag(flags haystack, flags needle) -> bool {
	return (static_cast<std::uint32_t>(haystack) & static_cast<std::uint32_t>(needle)) == static_cast<std::uint32_t>(needle);
}

auto gse::initialize(const flags engine_flags, const engine_config& config) -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	network::initialize();
	renderer::initialize();
	engine.initialize();
	engine.world.initialize();
}

auto gse::update(const flags engine_flags, const engine_config& config) -> void {
	add_timer("Engine::update", [] {
		system_clock::update();

		engine.world.update();

		physics::update(
			engine.world.registries()
		);

		renderer::update(
			engine.world.registries()
		);

		engine.update();
	});
}

auto gse::render(const flags engine_flags, const engine_config& config) -> void {
	add_timer("Engine::render", [] {
		renderer::render(
			engine.world.registries(),
			[&] {
				engine.world.render();
				engine.render();
			}
		);
	});
}

template <typename... Args>
auto gse::start(const flags engine_flags, const engine_config& config) -> void {
	(engine.add_hook<Args>(), ...);

    initialize(engine_flags, config);

	auto exit = make_scope_exit([] {
		engine.world.shutdown();
		renderer::shutdown();
		network::shutdown();
	});

    task::start([&] {
        while (!should_shutdown.load(std::memory_order_acquire)) {
			window::poll_events();

        	frame_sync::begin();

        	input::update();

            const bool do_render = has_flag(
				engine_flags, 
				flags::render
			);

			trace::scope(engine.id(), [&] {
				const std::size_t participants = 1 + (do_render ? 1 : 0);
				std::latch frame_done(participants);

				task::post(
					[&, engine_flags, config] {
						update(engine_flags, config);
						frame_done.count_down();
					}
				);

				if (do_render) {
					task::post(
						[&, engine_flags, config] {
							render(engine_flags, config);
							frame_done.count_down();
						}
					);
				}

				frame_done.wait();
			});

			frame_sync::end();
			trace::finalize_frame();
        }

        task::wait_idle();
    });
}

auto gse::shutdown() -> void {
	should_shutdown = true;
}
