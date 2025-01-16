export module gse.core.engine;

import std;

import gse.graphics.camera;

export namespace gse {
	void initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function);
	void run(const std::function<bool()>& update_function, const std::function<bool()>& render_function);

	/// Request the engine to shut down after the current frame.
	void request_shutdown();
	void block_shutdown_requests();

	void set_imgui_enabled(bool enabled);
	camera& get_camera();
}

import gse.core.clock;
import gse.core.main_clock;
import gse.core.object_registry;
import gse.core.scene_loader;
import gse.graphics.debug;
import gse.graphics.gui;
import gse.graphics.renderer2d;
import gse.graphics.renderer3d;
import gse.platform.glfw.input;
import gse.platform.glfw.window;
import gse.platform.perma_assert;

gse::camera& gse::get_camera() {
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

void gse::request_shutdown() {
	if (g_engine_shutdown_blocked) {
		return;
	}
	g_engine_state = engine_state::shutdown;
}

// Stops the engine from shutting down this frame
void gse::block_shutdown_requests() {
	g_engine_shutdown_blocked = true;
}

void gse::set_imgui_enabled(const bool enabled) {
	g_imgui_enabled = enabled;
}

void gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) {
	g_engine_state = engine_state::initializing;

	g_game_shutdown_function = shutdown_function;

	window::initialize();

	gui::initialize();

	if (g_imgui_enabled) debug::set_up_imgui();

	renderer3d::initialize();
	renderer2d::initialize();

	initialize_function();

	g_engine_state = engine_state::running;
}

void update(const std::function<bool()>& update_function) {

	if (g_imgui_enabled) gse::add_timer("Engine::update");

	gse::window::update();

	if (g_imgui_enabled) gse::debug::update_imgui();

	gse::main_clock::update();

	gse::scene_loader::update();

	gse::input::update(gse::main_clock::get_delta_time());

	if (!update_function()) {
		gse::request_shutdown();
	}

	gse::registry::periodically_clean_up_registry();

	if (g_imgui_enabled) gse::reset_timer("Engine::render");
}

void render(const std::function<bool()>& render_function) {
	if (g_imgui_enabled) gse::add_timer("Engine::render");

	gse::window::begin_frame();

	gse::scene_loader::render();

	if (!render_function()) {
		gse::request_shutdown();
	}

	if (g_imgui_enabled) gse::display_timers();
	if (g_imgui_enabled) gse::debug::render_imgui();

	gse::gui::render();

	gse::window::end_frame();

	if (g_imgui_enabled) gse::reset_timer("Engine::update");
}

void shutdown() {
	g_game_shutdown_function();
	gse::renderer2d::shutdown();
	gse::window::shutdown();
}

void gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) {
	assert_comment(g_engine_state == engine_state::running, "Engine is not initialized");

	scene_loader::set_engine_initialized(true);

	while (g_engine_state == engine_state::running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}