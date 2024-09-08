#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec3 cameraPos = {0,0,0};

} gameData;

Engine::Shader shaderProgram;

Game::Arena arena({1000, 1000, 1000});
Game::Player player;

bool Game::initializeGame() {
	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	shaderProgram.createShaderProgram(RESOURCES_PATH "Arena/grid.vert", RESOURCES_PATH "Arena/grid.frag");
	arena.initialize();
	player.initialize();

	return true;
}

bool Game::gameLogic(const float deltaTime) {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	// Disable mouse cursor
	glfwSetInputMode(Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Use shader
	shaderProgram.use();

	glm::mat4 view = player.getCamera().getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 1000000.0f);
	auto model = glm::mat4(1.0f);

	// Pass the matrices to the shader
	shaderProgram.setMat4("view", glm::value_ptr(view));
	shaderProgram.setMat4("projection", glm::value_ptr(projection));
	shaderProgram.setMat4("model", glm::value_ptr(model));

	player.update(deltaTime);
	player.render(view, projection);

	arena.render(view, projection);

	ImGui::Begin("Test Imgui");

	ImGui::InputFloat3("Camera Position", &player.getCamera().getPosition()[0]);

	ImGui::End();

	return true;
}

void Game::closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
