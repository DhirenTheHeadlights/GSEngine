#include "Engine/Core/Engine.h"

Engine::IDHandler Engine::idManager;
Engine::BroadPhaseCollisionHandler Engine::collisionHandler;
Engine::Shader Engine::shader;

void Engine::initialize() {
	shader.createShaderProgram(RESOURCES_PATH "Arena/grid.vert", RESOURCES_PATH "Arena/grid.frag");
}

void Engine::update(const float deltaTime, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) {
	shader.use();

	shader.setMat4("view", value_ptr(view));
	shader.setMat4("projection", value_ptr(projection));
	shader.setMat4("model", value_ptr(model));

	collisionHandler.update();
	Physics::updateEntities(deltaTime);
}

void Engine::render() {
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