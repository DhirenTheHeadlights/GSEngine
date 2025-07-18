export module gse.runtime:engine;

import std;

import :scene_loader;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.assert;

namespace gse {
	export struct engine : identifiable {};

	struct base_engine_hook final : hook<engine> {
		using hook::hook;

		auto update() -> void override {
			add_timer("Engine::update");

			platform::update();
			main_clock::update();
			scene_loader::update();
			gui::update();

			reset_timer("Engine::update");
		}

		auto render() -> void override {
			add_timer("Engine::render");

			scene_loader::render();

			reset_timer("Engine::render");
		}
	};

	hookable<engine> engine("Engine");

	export template <typename... Args>
	auto start() -> void;
}

export template <typename... Args>
auto gse::start() -> void {
	engine.add_hook(std::make_unique<base_engine_hook>(&engine));
	(engine.add_hook(std::make_unique<Args>(engine.id())), ...);

	engine.initialize();

	while (true) {
		engine.loop();
	}
}

