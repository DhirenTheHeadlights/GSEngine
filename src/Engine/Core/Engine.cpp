#include "Engine/Core/Engine.h"
#include "Engine/Core/Clock.h"
#include "Engine/Input/Input.h"

#define IMGUI 1

#if IMGUI
#include "Engine/Graphics/Debug.h"
#endif

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler Engine::collisionHandler;
Engine::Shader Engine::shader;

void Engine::initialize() {
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

	Clock::update();

	collisionHandler.update();

	Physics::updateEntities(Clock::getDeltaTime().asSeconds());

	Input::update();
}

void Engine::render() {
#if IMGUI
	Debug::renderImGui();
#endif
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