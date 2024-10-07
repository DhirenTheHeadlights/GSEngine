#include "Game/Player.h"

#include "Engine/Input/Input.h"

void Game::Player::initialize() {
	boundingBoxes.push_back(Engine::BoundingBox({ -10.f, -10.f, -10.f }, 10.f, 10.f, 10.f));

	wasd.insert({ GLFW_KEY_W, {0, 0, 1} });	
	wasd.insert({ GLFW_KEY_S, {0, 0, -1} });
	wasd.insert({ GLFW_KEY_A, {-1, 0, 0} });
	wasd.insert({ GLFW_KEY_D, {1, 0, 0} });

	camera.setPosition(boundingBoxes[0].getCenter());

	motionComponent.mass = Engine::Units::Pounds(180.f);
}

void Game::Player::update() {
	for (auto& bb : boundingBoxes) {
		bb.setPosition(motionComponent.position.as<Engine::Units::Meters>());
	}

	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held) {
			applyForce(&motionComponent, Engine::Vec3<Engine::Units::Newtons>(
				camera.getCameraDirectionRelativeToOrigin(direction) * 100.f)
			);
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !motionComponent.airborne) {
		applyForce(&motionComponent, Engine::Vec3<Engine::Units::Newtons>(0.f, 100000.f, 0.f ));
		motionComponent.airborne = true;
	}
	
	camera.setPosition(motionComponent.position.as<Engine::Units::Meters>());
	camera.updateCameraVectors();
	camera.processMouseMovement(Engine::Input::getMouse().delta);
}

void Game::Player::render(const glm::mat4& view, const glm::mat4& projection) {
	for (auto& bb : boundingBoxes) {
		drawBoundingBox(bb, view * projection, true);
	}
}