#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec3 cameraPos = {0,0,0};

} gameData;

Engine::Camera camera({0,0,0});
Engine::Shader shaderProgram;

Game::Arena arena({100, 100, 100});

bool Game::initializeGame() {
	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	shaderProgram.createShaderProgram(RESOURCES_PATH "Arena/grid.vert", RESOURCES_PATH "Arena/grid.frag");
	arena.initialize();

	return true;
}

bool Game::gameLogic(const float deltaTime) {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	// Disable mouse cursor
	//if (platform::)
	glfwSetInputMode(Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Use shader
	shaderProgram.use();

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 1000.0f);
	auto model = glm::mat4(1.0f);

	// Pass the matrices to the shader
	shaderProgram.setMat4("view", glm::value_ptr(view));
	shaderProgram.setMat4("projection", glm::value_ptr(projection));
	shaderProgram.setMat4("model", glm::value_ptr(model));

	// Camera
	camera.updateCameraVectors();
	camera.processMouseMovement(Platform::getMouseDelta().x, Platform::getMouseDelta().y);

	arena.render(view, projection);

	if (Platform::isButtonHeld(Platform::Button::A)) {
		camera.moveLeft(deltaTime * 100);
	}
	if (Platform::isButtonHeld(Platform::Button::D)) {
		camera.moveRight(deltaTime * 100);
	}
	if (Platform::isButtonHeld(Platform::Button::W)) {
		camera.moveForward(deltaTime * 100);
	}
	if (Platform::isButtonHeld(Platform::Button::S)) {
		camera.moveBackward(deltaTime * 100);
	}
	if (Platform::isButtonHeld(Platform::Button::Space)) {
		camera.moveUp(deltaTime * 100);
	}
	if (Platform::isButtonHeld(Platform::Button::LeftCtrl)) {
		camera.moveDown(deltaTime * 100);
	}

	std::cout << "Camera Position: " << camera.getPosition().x << ", " << camera.getPosition().y << ", " << camera.getPosition().z << std::endl;

	ImGui::Begin("Test Imgui");

	ImGui::DragFloat2("Positions", &gameData.cameraPos[0]);

	ImGui::End();

	return true;
}

void Game::closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
