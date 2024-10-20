#include <enet/enet.h> // Included first to avoid linking errors
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "Engine/Include/Core/Engine.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);

	while (!glfwWindowShouldClose(Engine::Platform::window)) {
		Engine::update(Game::update);
		Engine::render(Game::getCamera(), Game::render);
	}

	Engine::shutdown();

	return 0;
}
