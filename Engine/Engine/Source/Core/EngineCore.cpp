#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Graphics/Renderer.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

#if IMGUI == 0
#pragma message("IMGUI is set to 0")
#else
#pragma message("IMGUI is set to 1")
#endif


#if IMGUI
#include "Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idHandler;
Engine::SceneHandler Engine::sceneHandler(idHandler);

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
}

void Engine::requestShutdown() {
	engineState = EngineState::Shutdown;
}

void Engine::initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction) {
	engineState = EngineState::Initializing;

	gameShutdownFunction = shutdownFunction;

	Window::initialize();

#if IMGUI
	Debug::setUpImGui();
#endif

	initializeFunction();

	engineState = EngineState::Running;
}

namespace {
	void update(const std::function<bool()>& updateFunction) {
#if IMGUI
		Engine::addTimer("Engine::update");
#endif

		Engine::Window::update();

#if IMGUI
		Engine::Debug::updateImGui();
#endif

		Engine::MainClock::update();

		Engine::sceneHandler.update();

		Engine::Input::update();

		if (!updateFunction()) {
			Engine::requestShutdown();
		}

#if IMGUI
		Engine::resetTimer("Engine::render");
#endif
	}

	void render(const std::function<bool()>& renderFunction) {
		if (!Engine::Window::isFocused()) {
			return;
		}

#if IMGUI
		Engine::addTimer("Engine::render");
#endif

		Engine::Window::beginFrame();

		Engine::sceneHandler.render();

		if (!renderFunction()) {
			Engine::requestShutdown();
		}

#if IMGUI
		Engine::displayTimers();
		Engine::Debug::renderImGui();
#endif
		Engine::Window::endFrame();

		Engine::resetTimer("Engine::update");
	}

	void shutdown() {
		gameShutdownFunction();
		Engine::Window::shutdown();
	}
}

void Engine::run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	while (engineState == EngineState::Running && !Window::isWindowClosed()) {
		if (!Window::isFocused()) continue;

		update(updateFunction);
		render(renderFunction);
	}

	shutdown();
}
