#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Core/ObjectRegistry.h"
#include "Graphics/2D/Gui.h"
#include "Graphics/2D/Renderer2D.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

gse::scene_handler gse::g_scene_handler;

gse::camera& gse::get_camera() {
	return renderer3d::get_camera();
}

namespace {
	std::function<void()> g_game_shutdown_function = [] {};

	enum class engine_state : uint8_t {
		uninitialized,
		initializing,
		running,
		shutdown
	};

	auto g_engine_state = engine_state::uninitialized;

	bool g_engine_shutdown_blocked = false;
	bool g_imgui_enabled = false;
}

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

namespace {
	void update(const std::function<bool()>& update_function) {

		if (g_imgui_enabled) gse::add_timer("Engine::update");

		gse::window::update();

		if (g_imgui_enabled) gse::debug::update_imgui();

		gse::main_clock::update();

		gse::g_scene_handler.update();

		gse::input::update();

		if (!update_function()) {
			gse::request_shutdown();
		}

		gse::registry::periodically_clean_up_stale_lists();

		if (g_imgui_enabled) gse::reset_timer("Engine::render");
	}

	void render(const std::function<bool()>& render_function) {
		if (g_imgui_enabled) gse::add_timer("Engine::render");

		gse::window::begin_frame();

		gse::g_scene_handler.render();

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
}

void gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) {
	permaAssertComment(g_engine_state == engine_state::running, "Engine is not initialized");

	g_scene_handler.set_engine_initialized(true);

	while (g_engine_state == engine_state::running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}
