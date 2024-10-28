#include "Core/EngineCore.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Clock.h"
#include "Graphics/Renderer.h"
#include "Input/Input.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Platform/PermaAssert.h"
#include "Platform/Platform.h"

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

template <typename... ComponentTypes, typename Action>									
void handleComponent(const std::shared_ptr<Engine::Object>& object, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Namespace
		action(object->getComponent<ComponentTypes>()...);
	}
}

template <typename... ComponentTypes, typename SystemType, typename Action>				
void handleComponent(const std::shared_ptr<Engine::Object>& object, SystemType& system, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Class
		(system.*action)(object->getComponent<ComponentTypes>()...);
	}
}

void Engine::addObject(const std::weak_ptr<Object>& object) {
	if (const auto objectPtr = object.lock()) {
		handleComponent<Physics::MotionComponent>(objectPtr, Physics::addComponent);
		handleComponent<Physics::CollisionComponent, Physics::MotionComponent>(objectPtr, collisionHandler, &BroadPhaseCollisionHandler::addDynamicComponents);
		handleComponent<Physics::CollisionComponent>(objectPtr, collisionHandler, &BroadPhaseCollisionHandler::addObjectComponent);
		handleComponent<RenderComponent>(objectPtr, renderer, &Renderer::addRenderComponent);
		handleComponent<LightSourceComponent>(objectPtr, renderer, &Renderer::addLightSourceComponent);
	}
}

void Engine::removeObject(const std::weak_ptr<Object>& object) {
	if (const auto objectPtr = object.lock()) {
		handleComponent<Physics::MotionComponent>(objectPtr, Physics::removeComponent);
		handleComponent<Physics::CollisionComponent>(objectPtr, collisionHandler, &BroadPhaseCollisionHandler::removeComponents);
		handleComponent<RenderComponent>(objectPtr, renderer, &Renderer::removeComponent);
		handleComponent<LightSourceComponent>(objectPtr, renderer, &Renderer::removeLightSourceComponent);
	}
}
