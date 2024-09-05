#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
	class Camera {
	public:
		Camera(glm::vec3 initalPosition) : position(initalPosition) {}

		void moveUp(float deltaTime) { position += movementSpeed * deltaTime * up; }
		void moveDown(float deltaTime) { position -= movementSpeed * deltaTime * up; }
		void moveLeft(float deltaTime) { position -= glm::normalize(glm::cross(front, up)) * movementSpeed * deltaTime; }
		void moveRight(float deltaTime) { position += glm::normalize(glm::cross(front, up)) * movementSpeed * deltaTime; }
		void moveForward(float deltaTime) { position += movementSpeed * deltaTime * front; }
		void moveBackward(float deltaTime) { position -= movementSpeed * deltaTime * front; }

		void processMouseMovement(float xOffset, float yOffset);
		void updateCameraVectors();

		glm::mat4 getViewMatrix() const { return glm::lookAt(position, position + front, up); }

		glm::vec3 getPosition() const { return position; }
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