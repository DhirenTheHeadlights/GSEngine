#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"

namespace Engine {
	class Camera {
	public:
		Camera(const Vec3<Length>& initialPosition = { 0.f }) : position(initialPosition) {}

		void moveRelativeToOrigin(const Vec3<>& direction, float distance, float deltaTime);

		void processMouseMovement(glm::vec2& offset);
		void updateCameraVectors();

		void setPosition(const Vec3<Length>& position) { this->position = position; }

		glm::mat4 getViewMatrix() const;
		glm::mat4 getProjectionMatrix();
		Vec3<Length> getPosition() const;
		Vec3<> getCameraDirectionRelativeToOrigin(const Vec3<>& direction) const;
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
