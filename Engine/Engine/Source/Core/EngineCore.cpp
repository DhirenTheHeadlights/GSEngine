#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "Graphics/Renderer.h"
#include "Input/Input.h"
#include "Physics/System.h"
#include "Platform/PermaAssert.h"

#define IMGUI 1

#if IMGUI
#include "Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler collisionHandler;
Engine::Renderer renderer;

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

void requestShutdown() {
	engineState = EngineState::Shutdown;
}

void Engine::initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction) {
	engineState = EngineState::Initializing;

	gameShutdownFunction = shutdownFunction;

	Platform::initialize();
	renderer.initialize();

#if IMGUI
	Debug::setUpImGui();
#endif

	initializeFunction();

	engineState = EngineState::Running;
}

void update(const std::function<bool()>& updateFunction) {
	Engine::Platform::update();

#if IMGUI
	Engine::Debug::updateImGui();
#endif

	Engine::MainClock::update();

	collisionHandler.update();

	Engine::Physics::updateEntities();

	Engine::Input::update();

	if (!updateFunction()) {
		requestShutdown();
	}
}

void render(const std::function<bool()>& renderFunction) {
	Engine::Renderer::beginFrame();
	renderer.renderObjects();

	if (!renderFunction()) {
		requestShutdown();
	}

#if IMGUI
	Engine::Debug::renderImGui();
#endif
	Engine::Renderer::endFrame();
}

void shutdown() {
	gameShutdownFunction();
	glfwTerminate();
}

void Engine::run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction) {
	permaAssertComment(engineState == EngineState::Running, "Engine is not initialized");

	while (engineState == EngineState::Running && !glfwWindowShouldClose(Platform::window)) {
		update(updateFunction);
		render(renderFunction);
	}

	shutdown();
}

void Engine::addObject(const std::weak_ptr<StaticObject>& object) {
	collisionHandler.addObject(object);
}

void Engine::addObject(const std::weak_ptr<DynamicObject>& object) {
	collisionHandler.addObject(object);
	Physics::addObject(object);
}

void Engine::removeObject(const std::weak_ptr<StaticObject>& object) {
	collisionHandler.removeObject(object);
}

void Engine::removeObject(const std::weak_ptr<DynamicObject>& object) {
	collisionHandler.removeObject(object);
	Physics::removeObject(object);
}