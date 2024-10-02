#include "Game/Player.h"

#include "Engine/Input/Input.h"

void Game::Player::initialize() {
	boundingBoxes.push_back(Engine::BoundingBox({ -10.f, -10.f, -10.f }, 10.f, 10.f, 10.f));

	wasd.insert({ GLFW_KEY_W, {0, 0, 1} });	
	wasd.insert({ GLFW_KEY_S, {0, 0, -1} });
	wasd.insert({ GLFW_KEY_A, {-1, 0, 0} });
	wasd.insert({ GLFW_KEY_D, {1, 0, 0} });

	camera.setPosition(boundingBoxes[0].getCenter());

	motionComponent.mass = 1.f;
}

void Game::Player::update() {
	for (auto& bb : boundingBoxes) {
		bb.setPosition(motionComponent.position);
	}

	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held) {
			applyForce(&motionComponent, camera.getCameraDirectionRelativeToOrigin(direction * 100.f));
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !motionComponent.airborne) {
		applyForce(&motionComponent, { 0, 100000, 0 });
		motionComponent.airborne = true;
	}
	
	camera.setPosition(motionComponent.position);
	camera.updateCameraVectors();
	camera.processMouseMovement(Engine::Input::getMouse().delta);
}

void Game::Player::render(const glm::mat4& view, const glm::mat4& projection) {
	for (auto& bb : boundingBoxes) {
		drawBoundingBox(bb, view * projection, true);
	}
}