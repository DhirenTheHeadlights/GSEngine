#include "Game/Player.h"

#include "Engine/Input/Input.h"

using namespace Game;

void Player::initialize() {
	boundingBoxes.push_back(Engine::BoundingBox({ -10.f, -10.f, -10.f }, 10.f, 10.f, 10.f));

	movementKeys.insert({ GLFW_KEY_W, {0, 0, 1} });
	movementKeys.insert({ GLFW_KEY_S, {0, 0, -1} });
	movementKeys.insert({ GLFW_KEY_A, {1, 0, 0} });
	movementKeys.insert({ GLFW_KEY_D, {-1, 0, 0} });
	movementKeys.insert({ GLFW_KEY_SPACE, {0, 1, 0} });
	movementKeys.insert({ GLFW_KEY_LEFT_CONTROL, {0, -1, 0} });

	camera.setPosition(boundingBoxes[0].getCenter());
}

void Player::update(const float deltaTime) {
	for (auto& bb : boundingBoxes) {
		bb.setPosition(motionComponent.position);
	}

	for (auto& [key, direction] : movementKeys) {
		if (Engine::Input::getKeyboard().keys[key].held) {
			for (auto& bb : boundingBoxes) {
				motionComponent.acceleration = camera.getCameraDirectionRelativeToOrigin(direction * 10.f);
			}
		}
	}
	
	camera.setPosition(motionComponent.position);
	camera.updateCameraVectors();
	camera.processMouseMovement(Engine::Input::getMouse().delta);
}

void Player::render(const glm::mat4& view, const glm::mat4& projection) {
	for (auto& bb : boundingBoxes) {
		drawBoundingBox(bb, view * projection, true);
	}
}