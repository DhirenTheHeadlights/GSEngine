#include "Engine/Graphics/Camera.h"

void Engine::Camera::processMouseMovement(glm::vec2& offset) {
    offset *= mouseSensitivity;
    yaw += offset.x;
    pitch -= offset.y;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateCameraVectors();
}
 
void Engine::Camera::updateCameraVectors() {
	const Vec3<Length> newFront(
		cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
		sin(glm::radians(pitch)),
		sin(glm::radians(yaw)) * cos(glm::radians(pitch))
	);
    front = normalize(newFront);
    right = normalize(cross(front, worldUp));
	up = normalize(cross(right, front));
}