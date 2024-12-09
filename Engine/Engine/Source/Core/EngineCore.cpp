#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

Engine::SceneHandler Engine::sceneHandler;

Engine::Camera& Engine::getCamera() {
	return Renderer::getCamera();
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

void Engine::requestShutdown() {
	if (engineShutdownBlocked) {
		return;
	}
	engineState = EngineState::Shutdown;
}

// Stops the engine from shutting down this frame
void Engine::blockShutdownRequests() {
	engineShutdownBlocked = true;
}

void Engine::setImguiEnabled(const bool enabled) {
	imguiEnabled = enabled;
}

void Engine::initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction) {
	engineState = EngineState::Initializing;

	gameShutdownFunction = shutdownFunction;

	Window::initialize();

	if (imguiEnabled) Debug::setUpImGui();

	Renderer::initialize3d();

	initializeFunction();

	engineState = EngineState::Running;
}

namespace {
	void update(const std::function<bool()>& updateFunction) {

		if (imguiEnabled) Engine::addTimer("Engine::update");

		Engine::Window::update();

		if (imguiEnabled) Engine::Debug::updateImGui();

		Engine::MainClock::update();

		Engine::sceneHandler.update();

		Engine::Input::update();

		if (!updateFunction()) {
			Engine::requestShutdown();
		}

		if (imguiEnabled) Engine::resetTimer("Engine::render");
	}

	void render(const std::function<bool()>& renderFunction) {
		if (imguiEnabled) Engine::addTimer("Engine::render");

		Engine::Window::beginFrame();

		Engine::sceneHandler.render();

		if (!renderFunction()) {
			Engine::requestShutdown();
		}

		if (imguiEnabled) Engine::displayTimers();
		if (imguiEnabled) Engine::Debug::renderImGui();

		Engine::Window::endFrame();

		if (imguiEnabled) Engine::resetTimer("Engine::update");
	}

	void shutdown() {
		gameShutdownFunction();
		Engine::Window::shutdown();
	}
}

void Engine::run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	sceneHandler.setEngineInitialized(true);

	while (engineState == EngineState::Running && !Window::isWindowClosed()) {
		update(updateFunction);
		render(renderFunction);
	}

	shutdown();
}
