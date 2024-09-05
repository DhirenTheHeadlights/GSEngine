#define GLM_ENABLE_EXPERIMENTAL

#include "GameLayer.h"

struct GameData {
	glm::vec2 rectPos = {100,100};

} gameData;

gl2d::Renderer2D renderer;

bool initGame() {
	//initializing stuff for the renderer
	gl2d::init();
	renderer.create();

	//loading the saved data. Loading an entire structure like this makes savind game data very easy.
	Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	return true;
}

bool gameLogic(float deltaTime) {
	int w = 0; int h = 0;
	w = Platform::getFrameBufferSize().x; //window w
	h = Platform::getFrameBufferSize().y; //window h
	
	glViewport(0, 0, w, h);
	glClear(GL_COLOR_BUFFER_BIT); //clear screen

	renderer.updateWindowMetrics(w, h);

	if (Platform::isButtonHeld(Platform::Button::Left)) {
		gameData.rectPos.x -= deltaTime * 100;
	}
	if (Platform::isButtonHeld(Platform::Button::Right)) {
		gameData.rectPos.x += deltaTime * 100;
	}
	if (Platform::isButtonHeld(Platform::Button::Up)) {
		gameData.rectPos.y -= deltaTime * 100;
	}
	if (Platform::isButtonHeld(Platform::Button::Down)) {
		gameData.rectPos.y += deltaTime * 100;
	}

	gameData.rectPos = glm::clamp(gameData.rectPos, glm::vec2{0,0}, glm::vec2{w - 100,h - 100});
	renderer.renderRectangle({gameData.rectPos, 100, 100}, Colors_Blue);

	renderer.flush();

	ImGui::Begin("Test Imgui");

	ImGui::DragFloat2("Positions", &gameData.rectPos[0]);

	ImGui::End();

	return true;
}

void closeGame() {
	Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
