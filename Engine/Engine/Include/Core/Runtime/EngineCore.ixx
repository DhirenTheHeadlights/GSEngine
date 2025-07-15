export module gse.runtime:engine;

import std;

import :scene_loader;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.assert;

namespace gse {
	struct base_engine_hook final : hook<void> {
		using hook::hook;

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

			scene_loader::render();

			reset_timer("Engine::render");
		}
	};
}

export namespace gse {
	static constinit hookable<void> engine({ std::make_unique<base_engine_hook>(nullptr) });
	auto start() -> void;
}

auto gse::start() -> void {
	engine.initialize();

	while (true) {
		engine.loop();
	}
}

