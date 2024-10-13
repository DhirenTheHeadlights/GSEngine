#include "Engine/Core/EngineCore.h"

#include "Engine/Core/Clock.h"
#include "Engine/Input/Input.h"
#include "Engine/Platform/PermaAssert.h"

#define IMGUI 1

#if IMGUI
#include "Engine/Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler Engine::collisionHandler;
Engine::Shader Engine::shader;

void Engine::initialize() {
	permaAssertComment(glfwInit(), "err initializing glfw");
	glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

	Platform::initialize();

	shader.createShaderProgram(RESOURCES_PATH "Arena/grid.vert", RESOURCES_PATH "Arena/grid.frag");

#if IMGUI
	Debug::setUpImGui();
#endif
}

void Engine::update(const Camera& camera) {
	Platform::update();

	shader.setMat4("view", value_ptr(camera.getViewMatrix()));
	shader.setMat4("projection", value_ptr(glm::perspective(glm::radians(45.0f), static_cast<float>(Engine::Platform::getFrameBufferSize().x) / static_cast<float>(Engine::Platform::getFrameBufferSize().y), 0.1f, 1000.0f)));
	shader.setMat4("model", value_ptr(glm::mat4(1.0f)));
	shader.use();

#if IMGUI
	Debug::updateImGui();
#endif

	MainClock::update();

	collisionHandler.update();

	Physics::updateEntities();

	Input::update();

	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen
}

void Engine::render() {
#if IMGUI
	Debug::renderImGui();
#endif
	glfwSwapBuffers(Platform::window);
	glfwPollEvents();
}

void Engine::shutdown() {
}

void Engine::addObject(Object& object) {
	collisionHandler.addObject(object);
}

void Engine::addObject(DynamicObject& object) {
	collisionHandler.addObject(object);
	addMotionComponent(object.getMotionComponent());
}

void Engine::removeObject(Object& object) {
	collisionHandler.removeObject(object);
}

void Engine::removeObject(DynamicObject& object) {
	collisionHandler.removeObject(object);
	removeMotionComponent(object.getMotionComponent());
}