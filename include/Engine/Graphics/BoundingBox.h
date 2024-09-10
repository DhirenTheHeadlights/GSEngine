#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/epsilon.hpp>
#include <vector>
#include <iostream>

namespace Engine {
	struct CollisionInformation {
		glm::vec3 collisionNormal;
		float penetration;
		glm::vec3 collisionPoint;
	};

	struct BoundingBox {
		BoundingBox() = default;
		BoundingBox(const glm::vec3& upperBound, const glm::vec3& lowerBound) : upperBound(upperBound), lowerBound(lowerBound) {}

		~BoundingBox() {
			glDeleteVertexArrays(1, &gridVAO);
			glDeleteBuffers(1, &gridVBO);
		}

		glm::vec3 upperBound;
		glm::vec3 lowerBound;

		mutable CollisionInformation collisionInformation;

		// Debug rendering information
		std::vector<float> gridVertices;
		unsigned int gridVAO = 0, gridVBO = 0;

		void move(const glm::vec3& direction, const float distance, const float deltaTime) {
			upperBound += direction * distance * deltaTime;
			lowerBound += direction * distance * deltaTime;
		}

		void setPosition(const glm::vec3& center) {
			glm::vec3 halfSize = (upperBound - lowerBound) / 2.0f;
			upperBound = center + halfSize;
			lowerBound = center - halfSize;
		}

		glm::vec3 getCenter() const {
			return (upperBound + lowerBound) / 2.0f;
		}

		bool operator==(const BoundingBox& other) const {
			return glm::all(glm::epsilonEqual(upperBound, other.upperBound, 0.0001f)) &&
				glm::all(glm::epsilonEqual(lowerBound, other.lowerBound, 0.0001f));
		}
	};

	void drawBoundingBox(BoundingBox& boundingBox, const glm::mat4& viewProjectionMatrix, const bool moving, const glm::vec3& color = glm::vec3(1.0f));
}