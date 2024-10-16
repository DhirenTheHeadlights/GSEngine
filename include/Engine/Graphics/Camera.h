#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Physics/Vector/Vec3.h"
#include "Engine/Physics/Vector/Math.h"

namespace Engine {
	class Camera {
	public:
		Camera(const Vec3<Length>& initialPosition) : position(initialPosition) {}

		void moveRelativeToOrigin(const Vec3<Unitless>& direction, const float distance, const float deltaTime) {
			const auto normDirection = normalize(direction);
			const glm::vec3 cameraDirection =
				right * normDirection.x * right +
				normDirection.y * up +
				normDirection.z * front;
			position += cameraDirection * distance * movementSpeed * deltaTime;
		}

		void processMouseMovement(glm::vec2& offset);
		void updateCameraVectors();

		void setPosition(const Vec3<Length>& position) { this->position = position; }

		glm::mat4 getViewMatrix() const { return glm::lookAt(position, position + front, up); }
		Vec3<Length> getPosition() const { return position; }
		glm::vec3 getCameraDirectionRelativeToOrigin(Vec3<Unitless> direction) const {
			return direction * right + direction.y * up + direction.z * front * Vec3<Unitless>(1.f, 0.f, 1.f);
		}
	private:
		Vec3<Length> position;
		Vec3<Length> front = Vec3<Units::Meters>(0.0f, 0.0f, -1.0f);
		Vec3<Length> up = Vec3<Units::Meters>(0.0f, 1.0f, 0.0f);
		Vec3<Length> right = Vec3<Units::Meters>(1.0f, 0.0f, 0.0f);
		Vec3<Length> worldUp = Vec3<Units::Meters>(0.0f, 1.0f, 0.0f);

		float yaw = -90.0f;
		float pitch = 0.0f;

		float movementSpeed = 2.5f;
		float mouseSensitivity = 0.1f;
	};
}
