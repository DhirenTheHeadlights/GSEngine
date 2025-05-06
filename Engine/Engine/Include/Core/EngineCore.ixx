export module gse.core.engine;

import std;

import gse.core.clock;
import gse.core.main_clock;
import gse.core.timer;
import gse.core.object_registry;
import gse.core.scene_loader;
import gse.graphics;
import gse.platform;

export namespace gse {
	auto initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void;
	auto run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) -> void;

	/// Request the engine to shut down after the current frame.
	auto request_shutdown() -> void;
	auto block_shutdown_requests() -> void;

	auto set_imgui_enabled(bool enabled) -> void;
	auto get_camera() -> camera&;
}

auto gse::get_camera() -> camera& {
	return renderer3d::get_camera();
}

std::function<void()> g_game_shutdown_function = [] {};

enum class engine_state : std::uint8_t {
	uninitialized,
	initializing,
	running,
	shutdown
};

auto g_engine_state = engine_state::uninitialized;

bool g_engine_shutdown_blocked = false;
bool g_imgui_enabled = false;

auto gse::request_shutdown() -> void {
	if (g_engine_shutdown_blocked) {
		return;
	}
	g_engine_state = engine_state::shutdown;
}

// Stops the engine from shutting down this frame
auto gse::block_shutdown_requests() -> void {
	g_engine_shutdown_blocked = true;
}

auto gse::set_imgui_enabled(const bool enabled) -> void {
	g_imgui_enabled = enabled;
}

auto gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void {
	g_engine_state = engine_state::initializing;

	g_game_shutdown_function = shutdown_function;

	renderer::initialize();
	gui::initialize();

	initialize_function();

	g_engine_state = engine_state::running;
}

auto update(const std::function<bool()>& update_function) -> void {
	if (g_imgui_enabled) gse::add_timer("Engine::update");

	gse::window::update();

	if (g_imgui_enabled) gse::debug::update_imgui();

	gse::gui::update();
	gse::main_clock::update();
	gse::scene_loader::update();
	gse::input::update();

	if (!update_function()) {
		gse::request_shutdown();
	}

	gse::registry::periodically_clean_up_registry();

	if (g_imgui_enabled) gse::reset_timer("Engine::render");
}

auto render(const std::function<bool()>& render_function) -> void {
	if (g_imgui_enabled) gse::add_timer("Engine::render");

	gse::renderer::begin_frame();

	gse::scene_loader::render();

	if (!render_function()) {
		gse::request_shutdown();
	}

	if (g_imgui_enabled) gse::display_timers();

	gse::gui::render();

	gse::renderer::end_frame();

	if (g_imgui_enabled) gse::reset_timer("Engine::update");
}

auto shutdown() -> void {
	g_game_shutdown_function();
	gse::renderer::shutdown();
}

auto gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) -> void {
	assert(g_engine_state == engine_state::running, "Engine is not initialized");

	scene_loader::set_engine_initialized(true);

	while (g_engine_state == engine_state::running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}

