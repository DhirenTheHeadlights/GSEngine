#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Physics/Surfaces.h"
#include "Engine/Physics/Units/Units.h"
#include "Engine/Physics/Vector/Vec3.h"

namespace Engine {
	struct CollisionInformation {
		bool colliding = false;
		glm::vec3 collisionNormal;
		float penetration;
		Vec3<Length> collisionPoint;
	};

	struct BoundingBox {
		BoundingBox() = default;

		// Only use this constructor if you know what you are doing
		BoundingBox(const Vec3<Length>& upperBound, const Vec3<Length>& lowerBound) : upperBound(upperBound), lowerBound(lowerBound) {}

		// Use this constructor for a centered bounding box
		BoundingBox(const Vec3<Length>& center, const Length& width, const Length& height, const Length& depth)
					: upperBound(center + Vec3<Length>(width / 2.f, height / 2.f, depth / 2.f)),
					  lowerBound(center - Vec3<Length>(width / 2.f, height / 2.f, depth / 2.f)) {}

		~BoundingBox() {
			glDeleteVertexArrays(1, &gridVAO);
			glDeleteBuffers(1, &gridVBO);
		}

		Vec3<Length> upperBound;
		Vec3<Length> lowerBound;

		bool setGrid = false;

		mutable CollisionInformation collisionInformation = {};

		// Debug rendering information
		std::vector<float> gridVertices;
		unsigned int gridVAO = 0, gridVBO = 0;

		void setPosition(const Vec3<Length>& center) {
			const Vec3<Length> halfSize = (upperBound - lowerBound) / 2.0f;
			upperBound = center + halfSize;
			lowerBound = center - halfSize;
		}

		Vec3<Length> getCenter() const {
			return (upperBound + lowerBound) / 2.0f;
		}

		Vec3<Length> getSize() const {
			return upperBound - lowerBound;
		}

		bool operator==(const BoundingBox& other) const {
			return lowerBound == other.lowerBound && upperBound == other.upperBound;
		}
	};

	void drawBoundingBox(BoundingBox& boundingBox, const glm::mat4& viewProjectionMatrix, const bool moving, const glm::vec3& color = glm::vec3(1.0f));
}
