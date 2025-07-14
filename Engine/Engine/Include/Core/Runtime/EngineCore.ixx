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

	hookable<void> engine;
}

namespace gse {
	auto gse::update(const std::function<void()>& update_function) -> void;
	auto gse::render(const std::function<void()>& render_function) -> void;
	auto gse::shutdown() -> void;

	struct base_engine_hook final : hook<void> {
		using hook::hook;

		auto initialize() -> void override {
			renderer::initialize();
		}

		auto update() -> void override {
			add_timer("Engine::update");

			platform::update();
			main_clock::update();
			scene_loader::update();
			gui::update();

			reset_timer("Engine::render");
		}

		auto render() -> void override {
			add_timer("Engine::render");

			renderer::render(
				[] {
					scene_loader::render();
				}
			);

			reset_timer("Engine::render");
		}
	};
}

auto gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void {
	engine.game_shutdown_function = shutdown_function;
	renderer::initialize();
	initialize_function();
}

auto gse::update(const std::function<void()>& update_function) -> void {
	add_timer("Engine::update");

	platform::update();
	main_clock::update();
	scene_loader::update();
	gui::update();

	reset_timer("Engine::render");
}

auto gse::render(const std::function<void()>& render_function) -> void {
	add_timer("Engine::render");

	renderer::render(
		[render_function] {
			scene_loader::render();
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

