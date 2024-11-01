#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"

std::shared_ptr<Game::Arena> arena;
std::shared_ptr<Game::Player> player;

bool Game::initialize() {
	arena = std::make_shared<Arena>();
	player = std::make_shared<Player>();

	arena->initialize();
	player->initialize();

	addObject(player);
	addObject(arena);

	return true;
}

bool Game::update() {
	Engine::Window::setMouseVisible(Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || Engine::Input::getKeyboard().keys[GLFW_KEY_N].toggled);

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_ENTER].pressed && Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_ALT].held) {
		Engine::Window::setFullScreen(!Engine::Window::isFullScreen());
	}

	player->update();

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
		if (close()) {
			std::cerr << "Game closed properly." << '\n';
			return false;
		}
		std::cerr << "Failed to close game properly." << '\n';
		return false;
	}

	return true;
}

bool Game::render() {
	player->render();
	arena->render();

	Engine::Debug::createWindow("Game Data");

	ImGui::Text("FPS: %d", Engine::MainClock::getFrameRate());

	Engine::Debug::printVector("Player Position", player->getComponent<Engine::Physics::MotionComponent>()->position.as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printVector("Player Bounding Box Position", player->getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].getCenter().as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printVector("Player Velocity", player->getComponent<Engine::Physics::MotionComponent>()->velocity.as<Engine::Units::MetersPerSecond>(), Engine::Units::MetersPerSecond::units());
	Engine::Debug::printVector("Player Acceleration", player->getComponent<Engine::Physics::MotionComponent>()->acceleration.as<Engine::Units::MetersPerSecondSquared>(), Engine::Units::MetersPerSecondSquared::units());

	Engine::Debug::printValue("Player Speed", player->getComponent<Engine::Physics::MotionComponent>()->getSpeed().as<Engine::Units::MilesPerHour>(), Engine::Units::MilesPerHour::units());

	Engine::Debug::printBoolean("Player Jetpack [J]", player->jetpack);

	ImGui::Text("Player Collision Information");

	const auto [colliding, collisionNormal, penetration, collisionPoint] = player->getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].collisionInformation;
	Engine::Debug::printBoolean("Player Colliding", colliding);
	Engine::Debug::printVector("Collision Normal", collisionNormal.rawVec3(), Engine::Units::Unitless::units());
	Engine::Debug::printValue("Penetration", penetration.as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printVector("Collision Point", collisionPoint.as<Engine::Units::Meters>(), Engine::Units::Meters::units());
	Engine::Debug::printBoolean("Player Airborne", player->getComponent<Engine::Physics::MotionComponent>()->airborne);
	Engine::Debug::printBoolean("Player Moving", player->getComponent<Engine::Physics::MotionComponent>()->moving);

	return true;
}

bool Game::close() {
	return true;
}
