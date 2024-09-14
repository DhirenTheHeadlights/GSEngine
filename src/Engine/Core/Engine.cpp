#include "Engine.h"

Engine::IDHandler Engine::idManager;
Engine::CollisionHandler<Engine::GameObject> Engine::collisionHandler;
Engine::Shader Engine::shader;

void Engine::initialize() {
	shader.createShaderProgram(RESOURCES_PATH "Arena/grid.vert", RESOURCES_PATH "Arena/grid.frag");
}

void Engine::update(const float deltaTime) {
	shader.use();
	collisionHandler.update();
}

void Engine::render() {
}

void Engine::shutdown() {
}