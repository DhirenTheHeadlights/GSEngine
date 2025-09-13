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

	auto initialize() -> void;
	auto update() -> void;
	auto render() -> void;

	engine engine("GSEngine");
	bool should_shutdown = false;
}

export namespace gse {
	template <typename... Args>
	auto start() -> void;
	auto shutdown() -> void;
}

template <typename... Args>
auto gse::start() -> void {
	(engine.add_hook(std::make_unique<Args>(&engine)), ...);

	initialize();

	while (!should_shutdown) {
		input::update([] {
			update();
			render();
		});
	}

	scene_loader::shutdown();
}

auto gse::initialize() -> void {
	scene_loader::initialize();
	engine.initialize();
}

auto gse::update() -> void {
	add_timer("Engine::update", [] {
		window::poll_events();

		main_clock::update();
		scene_loader::update();

		engine.update();
	});
}

auto gse::render() -> void {
	add_timer("Engine::render", [] {
		scene_loader::render([] {
			engine.render();
		});
	});
}

auto gse::shutdown() -> void {
	should_shutdown = true;
}





