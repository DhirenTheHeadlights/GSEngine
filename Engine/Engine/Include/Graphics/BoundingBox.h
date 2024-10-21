#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "BBRenderComponent.h"
#include "Physics/Surfaces.h"
#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"

namespace Engine {
	struct CollisionInformation {
		bool colliding = false;
		Vec3<Unitless> collisionNormal;
		Length penetration;
		Vec3<Length> collisionPoint;
	};

	struct BoundingBox {
		BoundingBox() = default;

		// Only use this constructor if you know what you are doing
		BoundingBox(const Vec3<Length>& upperBound, const Vec3<Length>& lowerBound)
			: upperBound(upperBound), lowerBound(lowerBound), renderComponent(lowerBound, upperBound) {}

		// Use this constructor for a centered bounding box
		BoundingBox(const Vec3<Length>& center, const Length& width, const Length& height, const Length& depth)
			: BoundingBox(center + Vec3<Length>(width / 2.f, height / 2.f, depth / 2.f),
						  center - Vec3<Length>(width / 2.f, height / 2.f, depth / 2.f)) {}

		Vec3<Length> upperBound;
		Vec3<Length> lowerBound;

		mutable CollisionInformation collisionInformation = {};

		BoundingBoxRenderComponent renderComponent;

		void render(const bool moving) {
			renderComponent.update(moving);
		}

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
	};

	Vec3<Length> getLeftBound(const BoundingBox& boundingBox);
	Vec3<Length> getRightBound(const BoundingBox& boundingBox);
	Vec3<Length> getFrontBound(const BoundingBox& boundingBox);
	Vec3<Length> getBackBound(const BoundingBox& boundingBox);
}
