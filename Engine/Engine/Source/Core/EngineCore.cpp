#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

gse::scene_handler gse::scene_handler;

gse::camera& gse::get_camera() {
	return renderer::get_camera();
}

namespace {
	std::function<void()> gameShutdownFunction = [] {};

	enum class EngineState : uint8_t {
		Uninitialized,
		Initializing,
		Running,
		Shutdown
	};

	auto engineState = EngineState::Uninitialized;

	bool engineShutdownBlocked = false;
	bool imguiEnabled = false;
}

void gse::request_shutdown() {
	if (engineShutdownBlocked) {
		return;
	}
	engineState = EngineState::Shutdown;
}

// Stops the engine from shutting down this frame
void gse::block_shutdown_requests() {
	engineShutdownBlocked = true;
}

void gse::set_imgui_enabled(const bool enabled) {
	imguiEnabled = enabled;
}

void gse::initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) {
	engineState = EngineState::Initializing;

	gameShutdownFunction = shutdown_function;

	window::initialize();

	if (imguiEnabled) debug::set_up_im_gui();

	renderer::initialize3d();

	initialize_function();

	engineState = EngineState::Running;
}

namespace {
	void update(const std::function<bool()>& updateFunction) {

		if (imguiEnabled) gse::add_timer("Engine::update");

		gse::window::update();

		if (imguiEnabled) gse::debug::update_im_gui();

		gse::MainClock::update();

		gse::scene_handler.update();

		gse::input::update();

		if (!updateFunction()) {
			gse::request_shutdown();
		}

		if (imguiEnabled) gse::reset_timer("Engine::render");
	}

	void render(const std::function<bool()>& renderFunction) {
		if (imguiEnabled) gse::add_timer("Engine::render");

		gse::window::begin_frame();

		gse::scene_handler.render();

		if (!renderFunction()) {
			gse::request_shutdown();
		}

		if (imguiEnabled) gse::display_timers();
		if (imguiEnabled) gse::debug::render_im_gui();

		gse::window::end_frame();

		if (imguiEnabled) gse::reset_timer("Engine::update");
	}

	void shutdown() {
		gameShutdownFunction();
		gse::window::shutdown();
	}
}

void gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	scene_handler.set_engine_initialized(true);

	while (engineState == EngineState::Running && !window::is_window_closed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}
