#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
	class Camera {
	public:
		Camera(glm::vec3 initialPosition) : position(initialPosition) {}

		void moveRelativeToOrigin(const glm::vec3 direction, const float distance, const float deltaTime) {
			glm::vec3 cameraDirection =
				direction.x * right +
				direction.y * up +
				direction.z * front;
			position += cameraDirection * distance * movementSpeed * deltaTime;
		}

		void processMouseMovement(glm::vec2 offset);
		void updateCameraVectors();

		void setPosition(const glm::vec3& position) { this->position = position; }

		glm::mat4 getViewMatrix() const { return glm::lookAt(position, position + front, up); }
		glm::vec3 getPosition() const { return position; }
		glm::vec3 getCameraDirectionRelativeToOrigin(glm::vec3 direction) const {
			return direction.x * right + direction.y * up + direction.z * front;
		}
	private:
		glm::vec3 position;
		glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

		float yaw = -90.0f;
		float pitch = 0.0f;

		float movementSpeed = 2.5f;
		float mouseSensitivity = 0.1f;
	};
}