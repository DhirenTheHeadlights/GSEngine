#define GLM_ENABLE_EXPERIMENTAL

#include "Game/Game.h"

#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Engine.h"
#include "Game/Arena.h"
#include "Game/Player.h"

struct GameData {
	glm::vec3 playerPosition = {0,0,0};

} gameData;

auto arena = std::make_shared<Game::Arena>();
auto player = std::make_shared<Game::Player>();

const Engine::Camera& Game::getCamera() {
	return player->getCamera();
}

bool Game::initialize() {
	// Loading the saved data. Loading an entire structure like this makes saving game data very easy.
	Engine::Platform::readEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));

	// Set up game
	arena->initialize();
	player->initialize();

	// Set player position
	//player.setPosition(gameData.playerPosition);

	addObject(player);
	addObject(arena);

	return true;
}

bool Game::update() {

	if (Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled) {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(Engine::Platform::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	const glm::mat4 view = player->getCamera().getViewMatrix();
	const glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(Engine::Platform::getFrameBufferSize().x) / static_cast<float>(Engine::Platform::getFrameBufferSize().y), 0.1f, 10000.0f);

	player->update();
	player->render(view, projection);

	arena->render(view, projection);

	ImGui::Begin("DEBUG");
	ImGui::SetWindowSize({ 500.f, 500.f });

	Engine::Debug::printVector("Player Position: ", player->getMotionComponent().position.as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printVector("Player Bounding Box Position: ", player->getBoundingBoxes()[0].getCenter().as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printVector("Player Velocity: ", player->getMotionComponent().velocity.as<Engine::Units::MetersPerSecond>(), Engine::Units::MetersPerSecond::units());
	Engine::Debug::printVector("Player Acceleration: ", player->getMotionComponent().acceleration.as<Engine::Units::MetersPerSecondSquared>(), Engine::Units::MetersPerSecondSquared::units());

	const auto [colliding, collisionNormal, penetration, collisionPoint] = player->getBoundingBoxes()[0].collisionInformation;
	ImGui::Text("Player Collision: %s", player->isColliding() ? "True" : "False");
	ImGui::Text("Player Collision Information: ");
	Engine::Debug::printVector("Collision Normal: ", collisionNormal.as<Engine::Units::Unitless>(), Engine::Units::Unitless::units());
	ImGui::Text("Collision Depth: %f", penetration.as<Engine::Units::Meters>());
	Engine::Debug::printVector("Collision Point: ", collisionPoint.as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	ImGui::Text("Airborne: %s", player->getMotionComponent().airborne ? "True" : "False");
	ImGui::Text("Moving: %s", player->getMotionComponent().moving ? "True" : "False");
	
	ImGui::End();

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
		return false;
	}

	return true;
}

void Game::close() {
	Engine::Platform::writeEntireFile(RESOURCES_PATH "gameData.data", &gameData, sizeof(GameData));
}
