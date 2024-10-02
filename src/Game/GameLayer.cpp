#define GLM_ENABLE_EXPERIMENTAL

#include "Game/GameLayer.h"

#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Clock.h"
#include "Engine/Core/Engine.h"

#include "Engine/Input/Input.h"
#include "Engine/Platform/PlatformFunctions.h"
#include "Game/Arena.h"
#include "Game/Player.h"

struct GameData {
	glm::vec3 playerPosition = {0,0,0};

} gameData;

Game::Arena arena;
Game::Player player;

const Engine::Camera& Game::getCamera() {
	return player.getCamera();
}

bool Game::initializeGame() {
	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Engine::Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	arena.initialize();
	player.initialize();

	// Set player position
	player.setPosition(gameData.playerPosition);

	addObject(player);
	addObject(arena);

	return true;
}

bool Game::gameLogic() {
	glViewport(0, 0, Engine::Platform::getFrameBufferSize().x, Engine::Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	if (Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled) {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	const glm::mat4 view = player.getCamera().getViewMatrix();
	const glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Engine::Platform::getFrameBufferSize().x) / static_cast<float>(Engine::Platform::getFrameBufferSize().y), 0.1f, 10000.0f);

	player.update();
	player.render(view, projection);

	arena.render(view, projection);

	ImGui::Begin("DEBUG");
	ImGui::SetWindowSize({ 500.f, 500.f });

	ImGui::InputFloat3("Camera Position", &player.getCamera().getPosition()[0]);
	ImGui::InputFloat3("Player Bounding Box Position", &player.getBoundingBoxes()[0].getCenter()[0]);
	ImGui::InputFloat3("Player Velocity", &player.getMotionComponent().velocity[0]);
	ImGui::InputFloat3("Player Acceleration", &player.getMotionComponent().acceleration[0]);

	const auto [colliding, collisionNormal, penetration, collisionPoint] = player.getBoundingBoxes()[0].collisionInformation;
	ImGui::Text("Player Collision: %s", player.isColliding() ? "True" : "False");
	ImGui::Text("Player Collision Information: ");
	ImGui::Text("Collision Normal: %f, %f, %f", collisionNormal.x, collisionNormal.y, collisionNormal.z);
	ImGui::Text("Collision Depth: %f", penetration);
	ImGui::Text("Collision Point: %f, %f, %f", collisionPoint.x, collisionPoint.y, collisionPoint.z);
	ImGui::Text("Airborne: %s", player.getMotionComponent().airborne ? "True" : "False");
	
	ImGui::End();

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
		return false;
	}

	return true;
}

void Game::closeGame() {
	Engine::Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
