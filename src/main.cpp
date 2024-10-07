#include <enet/enet.h> // Included first to avoid linking errors

// Third-Party Library Includes
#include <GLFW/glfw3.h>

// Project-Specific Includes
#include "Engine/Core/Engine.h"
#include "Engine/Platform/Platform.h"
#include "Game/Game.h"

int main() {
    //enableReportGlErrors();

	Engine::initialize();

    if (!Game::initialize()) {
        return 0;
    }

	while (!glfwWindowShouldClose(Engine::Platform::window)) {
		Engine::update(Game::getCamera());

		if (!Game::update()) {
			Game::close();
			return 0;
		}

		Engine::render();
	}

    Game::close();

	Engine::shutdown();

	return 0;
}
