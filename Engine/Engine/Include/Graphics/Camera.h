#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Physics/Vector/Math.h"
#include "Physics/Vector/Vec3.h"

namespace Engine {
	class Camera {
	public:
		Camera(const Vec3<Length>& initialPosition = { 0.f }) : position(initialPosition) {}

		void moveRelativeToOrigin(const Vec3<Unitless>& direction, const float distance, const float deltaTime) {
			const auto normDirection = normalize(direction).asDefaultUnits();
			const Vec3<Length> cameraDirection =
				right * normDirection.x +
				up * normDirection.y +
				front * normDirection.z;
			position += cameraDirection * distance * movementSpeed * deltaTime;
		}

		void processMouseMovement(glm::vec2& offset);
		void updateCameraVectors();

		void setPosition(const Vec3<Length>& position) { this->position = position; }

		glm::mat4 getViewMatrix() const { return glm::lookAt(position.as<Meters>(), (position + front).as<Meters>(), up.as<Meters>()); }
		Vec3<Length> getPosition() const { return position; }
		Vec3<Unitless> getCameraDirectionRelativeToOrigin(const Vec3<Unitless>& direction) const {
			return { right.as<Meters>() * direction.asDefaultUnits().x + up.as<Meters>() * direction.asDefaultUnits().y + front.as<Meters>() * direction.asDefaultUnits().z};
		}
	private:
		Vec3<Length> position;
		Vec3<Length> front = Vec3<Meters>(0.0f, 0.0f, -1.0f);
		Vec3<Length> up = Vec3<Meters>(0.0f, 1.0f, 0.0f);
		Vec3<Length> right = Vec3<Meters>(1.0f, 0.0f, 0.0f);
		Vec3<Length> worldUp = Vec3<Meters>(0.0f, 1.0f, 0.0f);

		float yaw = -90.0f;
		float pitch = 0.0f;

		float movementSpeed = 2.5f;
		float mouseSensitivity = 0.1f;
	};
}
