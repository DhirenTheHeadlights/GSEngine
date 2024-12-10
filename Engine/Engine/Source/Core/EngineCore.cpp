#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

gse::SceneHandler gse::scene_handler;

gse::Camera& gse::get_camera() {
	return renderer::getCamera();
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

	Window::initialize();

	if (imguiEnabled) Debug::setUpImGui();

	renderer::initialize3d();

	initialize_function();

	engineState = EngineState::Running;
}

namespace {
	void update(const std::function<bool()>& updateFunction) {

		if (imguiEnabled) gse::addTimer("Engine::update");

		gse::Window::update();

		if (imguiEnabled) gse::Debug::updateImGui();

		gse::MainClock::update();

		gse::scene_handler.update();

		gse::Input::update();

		if (!updateFunction()) {
			gse::request_shutdown();
		}

		if (imguiEnabled) gse::resetTimer("Engine::render");
	}

	void render(const std::function<bool()>& renderFunction) {
		if (imguiEnabled) gse::addTimer("Engine::render");

		gse::Window::beginFrame();

		gse::scene_handler.render();

		if (!renderFunction()) {
			gse::request_shutdown();
		}

		if (imguiEnabled) gse::displayTimers();
		if (imguiEnabled) gse::Debug::renderImGui();

		gse::Window::endFrame();

		if (imguiEnabled) gse::resetTimer("Engine::update");
	}

	void shutdown() {
		gameShutdownFunction();
		gse::Window::shutdown();
	}
}

void gse::run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	scene_handler.setEngineInitialized(true);

	while (engineState == EngineState::Running && !Window::isWindowClosed()) {
		update(update_function);
		render(render_function);
	}

	shutdown();
}
