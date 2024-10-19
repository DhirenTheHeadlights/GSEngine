#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Physics/Vector/Vec3.h"
#include "Engine/Physics/Vector/Math.h"

namespace Engine {
	class Camera {
	public:
		Camera(const Vec3<Length>& initialPosition = { 0.f }) : position(initialPosition) {}

		void moveRelativeToOrigin(const Vec3<Unitless>& direction, const float distance, const float deltaTime) {
			const auto normDirection = normalize(direction).rawVec3();
			const Vec3<Length> cameraDirection =
				right * normDirection.x +
				up * normDirection.y +
				front * normDirection.z;
			position += cameraDirection * distance * movementSpeed * deltaTime;
		}

		void processMouseMovement(glm::vec2& offset);
		void updateCameraVectors();

		void setPosition(const Vec3<Length>& position) { this->position = position; }

		glm::mat4 getViewMatrix() const { return glm::lookAt(position.as<Units::Meters>(), (position + front).as<Units::Meters>(), up.as<Units::Meters>()); }
		Vec3<Length> getPosition() const { return position; }
		Vec3<Unitless> getCameraDirectionRelativeToOrigin(const Vec3<Unitless>& direction) const {
			return { right.as<Units::Meters>() * direction.rawVec3().x + up.as<Units::Meters>() * direction.rawVec3().y + front.as<Units::Meters>() * direction.rawVec3().z};
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
