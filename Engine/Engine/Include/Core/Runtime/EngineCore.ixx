export module gse.runtime:engine;

import std;

import :scene_loader;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.assert;

namespace gse {
	export struct engine : hookable<engine> {
		engine(const std::string& name) : hookable(name) {}
	};

	struct base_engine_hook final : hook<engine> {
		using hook::hook;

		auto initialize() -> void override {
			scene_loader::initialize();
		}

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

	engine engine("GSEngine");
	static constinit bool should_shutdown = false;
}

export namespace gse {
	template <typename... Args>
	auto start() -> void {
		engine.add_hook(std::make_unique<base_engine_hook>(&engine));
		(engine.add_hook(std::make_unique<Args>(&engine)), ...);

		engine.initialize();
		engine.loop_while([]{ return !should_shutdown; });
	}

	auto shutdown() -> void {
		should_shutdown = true;
	}
}

