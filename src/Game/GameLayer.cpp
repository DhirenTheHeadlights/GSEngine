#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec3 cameraPos = {0,0,0};

} gameData;

Game::Arena arena({1000, 1000, 1000});
Game::Player player;

bool Game::initializeGame() {
	Engine::initialize();

	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	arena.initialize();
	player.initialize();

	// Set up collision handler
	Engine::collisionHandler.addObject(player);
	Engine::collisionHandler.addObject(arena);

	return true;
}

bool Game::gameLogic(const float deltaTime) {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	// Disable mouse cursor if middle mouse is not pressed
	if (Platform::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].held)
		glfwSetInputMode(Platform::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	else glfwSetInputMode(Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Update Engine
	Engine::update(deltaTime);

	glm::mat4 view = player.getCamera().getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 1000000.0f);
	auto model = glm::mat4(1.0f);

	// Pass the matrices to the shader
	Engine::shader.setMat4("view", glm::value_ptr(view));
	Engine::shader.setMat4("projection", glm::value_ptr(projection));
	Engine::shader.setMat4("model", glm::value_ptr(model));

	player.update(deltaTime);
	player.render(view, projection);

	arena.render(view, projection);

	ImGui::Begin("Test Imgui");

	ImGui::InputFloat3("Camera Position", &player.getCamera().getPosition()[0]);
	ImGui::InputFloat3("Player Bounding Box Position", &player.getBoundingBoxes()[0].getCenter()[0]);

	ImGui::End();

	return true;
}

void Game::closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
