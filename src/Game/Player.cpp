#include "Player.h"

using namespace Game;

void Player::initialize() {
	boundingBoxes.push_back(Engine::BoundingBox({ -10.f, -10.f, -10.f }, 10.f, 10.f, 10.f));

	movementKeys.insert({ GLFW_KEY_W, {0, 0, 1} });
	movementKeys.insert({ GLFW_KEY_S, {0, 0, -1} });
	movementKeys.insert({ GLFW_KEY_A, {-1, 0, 0} });
	movementKeys.insert({ GLFW_KEY_D, {1, 0, 0} });
	movementKeys.insert({ GLFW_KEY_SPACE, {0, 1, 0} });
	movementKeys.insert({ GLFW_KEY_LEFT_CONTROL, {0, -1, 0} });

	camera.setPosition(boundingBoxes[0].getCenter());
}

void Player::update(const float deltaTime) {
	for (auto& [key, direction] : movementKeys) {
		if (Platform::getKeyboard().keys[key].held) {
			for (auto& bb : boundingBoxes) {
				bb.move(camera.getCameraDirectionRelativeToOrigin(direction), 10.f, deltaTime);
			}
		}
	}

	// If the player is colliding with something, move the player back to the previous position
	for (auto& bb : boundingBoxes) {
		if (bb.collisionInformation.penetration > 0) {
			bb.move(-bb.collisionInformation.collisionNormal, bb.collisionInformation.penetration, deltaTime);
		}
	}
	
	camera.setPosition(boundingBoxes[0].getCenter());
	camera.updateCameraVectors();
	camera.processMouseMovement(Platform::getMouse().delta);
}

void Player::render(const glm::mat4& view, const glm::mat4& projection) {
	for (auto& bb : boundingBoxes) {
		drawBoundingBox(bb, view * projection, true);
	}
}