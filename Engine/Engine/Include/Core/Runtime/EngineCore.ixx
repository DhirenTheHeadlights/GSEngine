export module gse.runtime:engine;

import std;

import :registry;
import :scene_loader;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.assert;

export namespace gse {
	auto initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void;
	auto run(const std::function<void()>& update_function, const std::function<void()>& render_function) -> void;
}

namespace gse {
	struct engine {
		enum class state : std::uint8_t {
			uninitialized,
			initializing,
			running,
			shutdown
		};

		std::function<void()> game_shutdown_function;
		state state;
		registry registry;
		scene_loader loader;
	} engine;

	auto gse::update(const std::function<void()>& update_function) -> void;
	auto gse::render(const std::function<void()>& render_function) -> void;
	auto gse::shutdown() -> void;
}

auto gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void {
	engine.state = engine::state::initializing;
	engine.game_shutdown_function = shutdown_function;
	renderer::initialize();
	initialize_function();
	engine.state = engine::state::running;
}

auto gse::update(const std::function<void()>& update_function) -> void {
	add_timer("Engine::update");

	platform::update();
	main_clock::update();
	engine.loader.update();
	gui::update();

	reset_timer("Engine::render");
}

auto gse::render(const std::function<void()>& render_function) -> void {
	add_timer("Engine::render");

	renderer::render(
		[render_function] {
			engine.loader.render();
			render_function();
		},
		engine.registry.linked_objects<render_component>()
	);

	reset_timer("Engine::render");
}

auto gse::shutdown() -> void {
	engine.game_shutdown_function();
	renderer::shutdown();
}

auto gse::run(const std::function<void()>& update_function, const std::function<void()>& render_function) -> void {
	assert(engine.state == engine::state::running, "Engine is not initialized");

	while (engine.state == engine::state::running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}

