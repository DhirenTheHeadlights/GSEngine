#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec3 cameraPos = {0,0,0};

} gameData;

Engine::Camera camera({0,0,0});
Game::Arena arena({100, 100, 100});

bool Game::initGame() {
	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	arena.initialize();

	return true;
}

bool Game::gameLogic(float deltaTime) {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT); // Clear screen

	// Camera
	camera.updateCameraVectors();
	camera.processMouseMovement(Platform::getMouseDelta().x, Platform::getMouseDelta().y);

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

	// Arena
	arena.render(camera.getViewMatrix(), glm::perspective(glm::radians(45.0f), (float)Platform::getFrameBufferSize().x / (float)Platform::getFrameBufferSize().y, 0.1f, 100.0f));

	ImGui::Begin("Test Imgui");

	ImGui::DragFloat2("Positions", &gameData.cameraPos[0]);

	ImGui::End();

	return true;
}

void Game::closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
