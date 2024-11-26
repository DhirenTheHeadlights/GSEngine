#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"

std::shared_ptr<Game::Arena> arena;
std::shared_ptr<Game::Player> player;
std::shared_ptr<Engine::Box> box;
std::shared_ptr<Game::SphereLight> sphere;

bool inputHandlingEnabled = true;

void Game::setInputHandlingFlag(const bool enabled) {
	inputHandlingEnabled = enabled;
}

bool Game::initialize() {
	arena  = std::make_shared<Arena>();
	player = std::make_shared<Player>();
	box    = std::make_shared<Engine::Box>(Engine::Vec3<Engine::Meters>(20.f, -400.f, 20.f), Engine::Vec3<Engine::Meters>(20.f, 20.f, 20.f));
	sphere = std::make_shared<SphereLight>(Engine::Vec3<Engine::Meters>(0.f, 0.f, 0.f), Engine::meters(10.f));

	const auto scene1 = std::make_shared<Engine::Scene>();

	scene1->addObject(arena);
	scene1->addObject(player);
	scene1->addObject(box);
	scene1->addObject(sphere);

	Engine::sceneHandler.addScene(scene1, "Scene1");

	return true;
}

bool Game::update() {
	if (inputHandlingEnabled) {
		Engine::Window::setMouseVisible(Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || Engine::Input::getKeyboard().keys[GLFW_KEY_N].toggled);

		if (Engine::Input::getKeyboard().keys[GLFW_KEY_ENTER].pressed && Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_ALT].held) {
			Engine::Window::setFullScreen(!Engine::Window::isFullScreen());
		}

		if (Engine::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
			if (close()) {
				std::cerr << "Game closed properly." << '\n';
				return false;
			}
			std::cerr << "Failed to close game properly." << '\n';
			return false;
		}
	}

	return true;
}

bool Game::render() {
	if (const auto scene1Id = Engine::grabID("Scene1").lock()) {
		Engine::sceneHandler.activateScene(scene1Id);
	}

	Engine::Debug::addImguiCallback([] {
		ImGui::Begin("Game Data");

		ImGui::Text("FPS: %d", Engine::MainClock::getFrameRate());

		Engine::Debug::printVector("Player Position", player->getComponent<Engine::Physics::MotionComponent>()->position.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Player Bounding Box Position", player->getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].getCenter().as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Player Velocity", player->getComponent<Engine::Physics::MotionComponent>()->velocity.as<Engine::MetersPerSecond>(), Engine::MetersPerSecond::UnitName);
		Engine::Debug::printVector("Player Acceleration", player->getComponent<Engine::Physics::MotionComponent>()->acceleration.as<Engine::MetersPerSecondSquared>(), Engine::MetersPerSecondSquared::UnitName);

		Engine::Debug::printValue("Player Speed", player->getComponent<Engine::Physics::MotionComponent>()->getSpeed().as<Engine::MilesPerHour>(), Engine::MilesPerHour::UnitName);

		Engine::Debug::printBoolean("Player Jetpack [J]", player->jetpack);

		ImGui::Text("Player Collision Information");

		const auto [colliding, collisionNormal, penetration, collisionPoint] = player->getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].collisionInformation;
		Engine::Debug::printBoolean("Player Colliding", colliding);
		Engine::Debug::printVector("Collision Normal", collisionNormal.asDefaultUnits(), "");
		Engine::Debug::printValue("Penetration", penetration.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Collision Point", collisionPoint.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printBoolean("Player Airborne", player->getComponent<Engine::Physics::MotionComponent>()->airborne);
		Engine::Debug::printBoolean("Player Moving", player->getComponent<Engine::Physics::MotionComponent>()->moving);
		ImGui::End();
		});

	return true;
}

bool Game::close() {
	return true;
}
