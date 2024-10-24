#include "Core/EngineCore.h"

#include "Core/Clock.h"
#include "GLFW/glfw3.h"
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

template <typename SystemType, typename Action, typename... ComponentTypes>
void handleClassComponents(const std::weak_ptr<Engine::Object>& object, const SystemType& system, Action action) {
	if (const auto objectPtr = object.lock()) {
		if ((objectPtr->getComponent<ComponentTypes>() && ...)) {
			(system.*action)(objectPtr->getComponent<ComponentTypes>()...);
		}
	}
}

template <typename Action, typename... ComponentTypes>
void handleNamespaceComponents(const std::weak_ptr<Engine::Object>& object, Action action) {
	if (const auto objectPtr = object.lock()) {
		if ((objectPtr->getComponent<ComponentTypes>() && ...)) {
			action(objectPtr->getComponent<ComponentTypes>()...);
		}
	}
}

void Engine::addObject(const std::weak_ptr<Object>& object) {
	handleNamespaceComponents<Physics::MotionComponent>(object, Physics::addComponent);
	handleClassComponents<Physics::CollisionComponent, Physics::MotionComponent>(object, collisionHandler, &BroadPhaseCollisionHandler::addComponents);
	handleClassComponents<RenderComponent>(object, renderer, &Renderer::addComponent);
}

void Engine::removeObject(const std::weak_ptr<Object>& object) {
	handleNamespaceComponents<Physics::MotionComponent>(object, Physics::removeComponent);
	handleClassComponents<Physics::CollisionComponent>(object, collisionHandler, &BroadPhaseCollisionHandler::removeComponents);
	handleClassComponents<RenderComponent>(object, renderer, &Renderer::removeComponent);
}