#include "Player.h"

using namespace Game;

void Player::initialize() {

}

void Player::update(float deltaTime) {
	if (Platform::getKeyboard().keys[GLFW_KEY_A].held) {
		camera.moveRelativeToOrigin({ -1, 0, 0 }, 100.f, deltaTime);
	}
	if (Platform::getKeyboard().keys[GLFW_KEY_D].held) {
		camera.moveRelativeToOrigin({ 1, 0, 0 }, 100.f, deltaTime);
	}
	if (Platform::getKeyboard().keys[GLFW_KEY_W].held) {
		camera.moveRelativeToOrigin({ 0, 0, 1 }, 100.f, deltaTime);
	}
	if (Platform::getKeyboard().keys[GLFW_KEY_S].held) {
		camera.moveRelativeToOrigin({ 0, 0, -1 }, 100.f, deltaTime);
	}
	if (Platform::getKeyboard().keys[GLFW_KEY_SPACE].held) {
		camera.moveRelativeToOrigin({ 0, 1, 0 }, 100.f, deltaTime);
	}
	if (Platform::getKeyboard().keys[GLFW_KEY_LEFT_CONTROL].held) {
		camera.moveRelativeToOrigin({ 0, -1, 0 }, 100.f, deltaTime);
	}

	camera.updateCameraVectors();
	camera.processMouseMovement(Platform::getMouse().delta);
}

void Player::render(const glm::mat4& view, const glm::mat4& projection) const {
	
}