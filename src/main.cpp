#include <enet/enet.h> // Included first to avoid linking errors
#include <GLFW/glfw3.h>
#include "Engine/Core/Engine.h"
#include "Game/Game.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);

	while (!glfwWindowShouldClose(Engine::Platform::window)) {
		Engine::update(Game::update);
		Engine::render(Game::getCamera(), Game::render);
	}

	Engine::shutdown();

	return 0;
}
