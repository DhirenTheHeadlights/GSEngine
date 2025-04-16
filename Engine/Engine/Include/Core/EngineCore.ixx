export module gse.core.engine;

import std;

import gse.graphics.camera;
import gse.core.clock;
import gse.core.main_clock;
import gse.core.system_backend;
import gse.core.timer;
import gse.platform.perma_assert;
import gse.platform.glfw.window;

export namespace gse {
	auto initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void;
	auto run(const std::function<bool()>& update_function = {}, const std::function<bool()>& render_function = {}) -> void;

	/// Request the engine to shut down after the current frame.
	auto request_shutdown() -> void;
	auto block_shutdown_requests() -> void;

	auto set_imgui_enabled(bool enabled) -> void;
}

std::function<void()> g_game_shutdown_function;

enum class engine_state : std::uint8_t {
	uninitialized,
	initializing,
	running,
	shutdown
};

auto g_engine_state = engine_state::uninitialized;

auto gse::request_shutdown() -> void {
	g_engine_state = engine_state::shutdown;
}

auto gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void {
	g_engine_state = engine_state::initializing;

	g_game_shutdown_function = shutdown_function;

	systems::initialize();

	initialize_function();

	g_engine_state = engine_state::running;
}

auto update(const std::function<bool()>& update_function) -> void {

	if (g_imgui_enabled) gse::add_timer("Engine::update");

	gse::systems::update();

	if (!update_function()) {
		gse::request_shutdown();
	}

	gse::registry::periodically_clean_up_registry();

	if (g_imgui_enabled) gse::reset_timer("Engine::render");
}

auto render(const std::function<bool()>& render_function) -> void {
	if (g_imgui_enabled) gse::add_timer("Engine::render");

	gse::window::begin_frame();

	gse::scene_loader::render();

	if (!render_function()) {
		gse::request_shutdown();
	}

	if (g_imgui_enabled) gse::display_timers();
	if (g_imgui_enabled) gse::debug::render();

	gse::gui::render();

	gse::window::end_frame();

	if (g_imgui_enabled) gse::reset_timer("Engine::update");
}

auto shutdown() -> void {
	g_game_shutdown_function();
	gse::renderer2d::shutdown();
	gse::window::shutdown();
}

auto gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) -> void {
	assert_comment(g_engine_state == engine_state::running, "Engine is not initialized");

	scene_loader::set_engine_initialized(true);

	while (g_engine_state == engine_state::running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}

