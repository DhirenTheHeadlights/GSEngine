#include "Engine/Core/EngineCore.h"

#include "Engine/Core/Clock.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/Input.h"
#include "Engine/Physics/System.h"
#include "Engine/Platform/PermaAssert.h"

#define IMGUI 1

#if IMGUI
#include "Engine/Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler collisionHandler;
Engine::Renderer renderer;

std::function<void()> gameShutdownFunction = [] {};

void Engine::initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction) {
	gameShutdownFunction = shutdownFunction;

	permaAssertComment(glfwInit(), "Error initializing GLFW");
	glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

	Platform::initialize();
	renderer.initialize();

#if IMGUI
	Debug::setUpImGui();
#endif

	initializeFunction();
}

void Engine::update(const std::function<bool()>& updateFunction) {
	Platform::update();

#if IMGUI
	Debug::updateImGui();
#endif

	MainClock::update();

	collisionHandler.update();

	Physics::updateEntities();

	Input::update();

	if (!updateFunction()) {
		shutdown();
	}
}

void Engine::render(const Camera& camera, const std::function<bool()>& renderFunction) {
	Renderer::beginFrame();
	renderer.setCameraInformation(camera);
	renderer.renderObjects();

	if (!renderFunction()) {
		shutdown();
	}

#if IMGUI
	Debug::renderImGui();
#endif
	Renderer::endFrame();
}

void Engine::shutdown() {
	gameShutdownFunction();
	glfwTerminate();
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