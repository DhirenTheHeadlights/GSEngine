#include "Core/EngineCore.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Clock.h"
#include "Graphics/Renderer.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

#define IMGUI 1

#if IMGUI
#include "Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler collisionHandler;
Engine::Renderer renderer;
Engine::Physics::System physicsSystem;

Engine::Camera& Engine::getCamera() {
	return renderer.getCamera();
}

std::function<void()> gameShutdownFunction = [] {};

enum class EngineState {
	Uninitialized,
	Initializing,
	Running,
	Shutdown
};

auto engineState = EngineState::Uninitialized;

void Engine::requestShutdown() {
	engineState = EngineState::Shutdown;
}

void Engine::initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction) {
	engineState = EngineState::Initializing;

	gameShutdownFunction = shutdownFunction;

	Window::initialize();
	renderer.initialize();

#if IMGUI
	Debug::setUpImGui();
#endif

	initializeFunction();

	engineState = EngineState::Running;
}

void update(const std::function<bool()>& updateFunction) {
	Engine::addTimer("Engine::update");

	Engine::Window::update();

#if IMGUI
	Engine::Debug::updateImGui();
#endif

	Engine::MainClock::update();

	collisionHandler.update();

	physicsSystem.update();

	Engine::Input::update();

	if (!updateFunction()) {
		Engine::requestShutdown();
	}

	Engine::resetTimer("Engine::render");
}

void render(const std::function<bool()>& renderFunction) {
	Engine::addTimer("Engine::render");

	Engine::Window::beginFrame();
	renderer.renderObjects();
	Engine::displayTimers();

	if (!renderFunction()) {
		Engine::requestShutdown();
	}

#if IMGUI
	Engine::Debug::renderImGui();
#endif
	Engine::Window::endFrame();

	Engine::resetTimer("Engine::update");
}

void shutdown() {
	gameShutdownFunction();
	Engine::Window::shutdown();
}

void Engine::run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	while (engineState == EngineState::Running && !Window::isWindowClosed()) {
		update(updateFunction);
		render(renderFunction);
	}

	shutdown();
}
