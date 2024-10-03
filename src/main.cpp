#include <enet/enet.h> // Included first to avoid linking errors

// Third-Party Library Includes
#include <GLFW/glfw3.h>

// Project-Specific Includes
#include "Engine/Core/Engine.h"
#include "Engine/Platform/PlatformFunctions.h"
#include "Engine/Platform/PlatformTools.h"
#include "Game/GameLayer.h"

int main() {
    permaAssertComment(glfwInit(), "err initializing glfw");
    glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

    //enableReportGlErrors();

	Engine::initialize();

    if (!Game::initializeGame()) {
        return 0;
    }

	while (!glfwWindowShouldClose(Engine::Platform::window)) {
		Engine::update(Game::getCamera());

		if (!Game::gameLogic()) {
			Game::closeGame();
			return 0;
		}

		Engine::render();
	}

    Game::closeGame();

	Engine::shutdown();
}
