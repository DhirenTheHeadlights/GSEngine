#include "Graphics/3D/Camera.h"

#include "Physics/Vector/Math.h"
#include "Platform/GLFW/Window.h"

void gse::Camera::moveRelativeToOrigin(const Vec3<>& direction, const float distance, const float deltaTime) {
	const auto normDirection = normalize(direction).asDefaultUnits();
	const Vec3<Length> cameraDirection =
		right * normDirection.x +
		up * normDirection.y +
		front * normDirection.z;
	position += cameraDirection * distance * movementSpeed * deltaTime;
}

void gse::Camera::processMouseMovement(glm::vec2& offset) {
    offset *= mouseSensitivity;
    yaw += offset.x;
    pitch -= offset.y;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateCameraVectors();
}
 
void gse::Camera::updateCameraVectors() {
	const Vec3<Length> newFront(
		cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
		sin(glm::radians(pitch)),
		sin(glm::radians(yaw)) * cos(glm::radians(pitch))
	);
    front = normalize(newFront);
    right = normalize(cross(front, worldUp));
	up = normalize(cross(right, front));
}

glm::mat4 gse::Camera::getViewMatrix() const {
	return lookAt(position.as<Meters>(), (position + front).as<Meters>(), up.as<Meters>());
}

glm::mat4 gse::Camera::getProjectionMatrix() {
	const glm::vec2 viewPortSize = Window::getFrameBufferSize();
	const float aspectRatio = viewPortSize.x / viewPortSize.y;
	return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10000.f);
}

gse::Vec3<gse::Length> gse::Camera::getPosition() const {
	return position;
}

gse::Vec3<> gse::Camera::getCameraDirectionRelativeToOrigin(const Vec3<>& direction) const {
	return { right.as<Meters>() * direction.asDefaultUnits().x + up.as<Meters>() * direction.asDefaultUnits().y + front.as<Meters>() * direction.asDefaultUnits().z };
}
