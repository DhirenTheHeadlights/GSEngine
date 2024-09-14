#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>

#include "Engine.h"

#include "Arena.h"
#include "Input.h"
#include "PlatformFunctions.h"
#include "Player.h"

struct GameData {
	glm::vec3 playerPosition = {0,0,0};

} gameData;

Game::Arena arena;
Game::Player player;

bool Game::initializeGame() {
	Engine::initialize();

	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Engine::Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	arena.initialize();
	player.initialize();

	// Set player position
	//player.setPosition(gameData.playerPosition);

	// Set up collision handler
	Engine::collisionHandler.addObject(player);
	Engine::collisionHandler.addObject(arena);

	return true;
}

bool Game::gameLogic(const float deltaTime) {
	glViewport(0, 0, Engine::Platform::getFrameBufferSize().x, Engine::Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	if (Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled) {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	glm::mat4 view = player.getCamera().getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Engine::Platform::getFrameBufferSize().x) / static_cast<float>(Engine::Platform::getFrameBufferSize().y), 0.1f, 1000.0f);
	auto model = glm::mat4(1.0f);

	// Update Engine
	Engine::update(deltaTime);

	// Pass the matrices to the shader
	Engine::shader.setMat4("view", glm::value_ptr(view));
	Engine::shader.setMat4("projection", glm::value_ptr(projection));
	Engine::shader.setMat4("model", glm::value_ptr(model));

	player.update(deltaTime);
	player.render(view, projection);

	arena.render(view, projection);

	ImGui::Begin("DEBUG");

	ImGui::InputFloat3("Camera Position", &player.getCamera().getPosition()[0]);
	ImGui::InputFloat3("Player Bounding Box Position", &player.getBoundingBoxes()[0].getCenter()[0]);

	Engine::CollisionInformation playerColInfo = player.getBoundingBoxes()[0].collisionInformation;
	ImGui::Text("Player Collision: %s", player.isColliding() ? "True" : "False");
	ImGui::Text("Player Collision Information: ");
	ImGui::Text("Collision Normal: %f, %f, %f", playerColInfo.collisionNormal.x, playerColInfo.collisionNormal.y, playerColInfo.collisionNormal.z);
	ImGui::Text("Collision Depth: %f", playerColInfo.penetration);
	ImGui::Text("Collision Point: %f, %f, %f", playerColInfo.collisionPoint.x, playerColInfo.collisionPoint.y, playerColInfo.collisionPoint.z);

	ImGui::Text("Arena Bounding Box Positions: ");
	for (int i = 0; i < arena.getBoundingBoxes().size(); i++) {
		ImGui::InputFloat3(("Arena Bounding Box " + std::to_string(i) + " Position").c_str(), &arena.getBoundingBoxes()[i].getCenter()[0]);
	}
	
	ImGui::End();

	return true;
}

void Game::closeGame() {
	Engine::Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
