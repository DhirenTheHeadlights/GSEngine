#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec3 cameraPos = {0,0,0};

} gameData;

Engine::Camera camera({0,0,0});

bool initGame() {
	// Loading the saved data. Loading an entire structure like this makes savind game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	return true;
}

bool gameLogic(float deltaTime) {
	int w = 0; int h = 0;
	w = Platform::getFrameBufferSize().x; //window w
	h = Platform::getFrameBufferSize().y; //window h
	
	glViewport(0, 0, w, h);
	glClear(GL_COLOR_BUFFER_BIT); //clear screen

	// Camera
	camera.updateCameraVectors();
	//camera.processMouseMovement(Platform::getMouseDelta().x, Platform::getMouseDelta().y);

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

void closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
