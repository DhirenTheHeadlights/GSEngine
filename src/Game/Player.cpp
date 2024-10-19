#include "Game/Player.h"

#include "Engine/Input/Input.h"
#include "Engine/Physics/System.h"

void Game::Player::initialize() {
	Engine::Units::Feet height = 6.0f;
	Engine::Units::Feet width = 3.0f;
	boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(-10.f, -10.f, -10.f), height, width, width);

	wasd.insert({ GLFW_KEY_W, {0.f, 0.f, 1.f} });
	wasd.insert({ GLFW_KEY_S, {0.f, 0.f, -1.f} });
	wasd.insert({ GLFW_KEY_A, {-1.f, 0.f, 0.f} });
	wasd.insert({ GLFW_KEY_D, {1.f, 0.f, 0.f} });

	motionComponent.mass = Engine::Units::Pounds(180.f);
	motionComponent.maxSpeed = maxSpeed;
	motionComponent.selfControlled = true;
}

void Game::Player::update() {
	if (Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		motionComponent.maxSpeed = shiftMaxSpeed;
	}
	else {
		motionComponent.maxSpeed = maxSpeed;
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].held) {
		applyForce(&motionComponent, Engine::Vec3<Engine::Units::Newtons>(0.f, jetpackForce, 0.f));
	}

	for (auto& bb : boundingBoxes) {
		bb.setPosition(motionComponent.position);
	}

	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held && !motionComponent.airborne) {
			applyForce(&motionComponent, Engine::Vec3<Engine::Units::Newtons>(
				camera.getCameraDirectionRelativeToOrigin(direction) * 1000000.f * Engine::Vec3<Engine::Unitless>(1.f, 0.f, 1.f))
			);
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !motionComponent.airborne) {
		applyForce(&motionComponent, Engine::Vec3<Engine::Units::Newtons>(0.f, 100000.f, 0.f ));
		motionComponent.airborne = true;
	}
	
	camera.setPosition(motionComponent.position + Engine::Vec3<Engine::Units::Feet>(0.f, 5.f, 0.f));
	camera.updateCameraVectors();
	camera.processMouseMovement(Engine::Input::getMouse().delta);
}

void Game::Player::render() {
	for (auto& bb : boundingBoxes) {
		bb.render(true);
	}
}