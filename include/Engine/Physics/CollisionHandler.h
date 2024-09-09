#pragma once

#include <vector>
#include <concepts>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

namespace Engine {
	struct CollisionInformation {
		glm::vec3 collisionNormal;
		float penetration;
		glm::vec3 collisionPoint;
	};

	struct BoundingBox {
		BoundingBox() = default;
		BoundingBox(const glm::vec3& corner1, const glm::vec3& corner2) : topCorner(corner1), bottomCorner(corner2) {}

		glm::vec3 topCorner;
		glm::vec3 bottomCorner;

		mutable CollisionInformation collisionInformation;

		void move(const glm::vec3& direction, const float distance, const float deltaTime) {
			topCorner += direction * distance * deltaTime;
			bottomCorner += direction * distance * deltaTime;
		}

		void setPosition(const glm::vec3& center) {
			glm::vec3 halfSize = (topCorner - bottomCorner) / 2.0f;
			topCorner = center + halfSize;
			bottomCorner = center - halfSize;
		}

		glm::vec3 getCenter() const {
			return (topCorner + bottomCorner) / 2.0f;
		}

		bool operator==(const BoundingBox& other) const {
			return glm::all(glm::epsilonEqual(topCorner, other.topCorner, 0.0001f)) &&
				   glm::all(glm::epsilonEqual(bottomCorner, other.bottomCorner, 0.0001f));
		}
	};

	template <typename T>
	concept Collidable = requires(T object, bool isColliding) {
		{ object.isColliding() } -> std::same_as<bool>;
		{ object.setIsColliding(isColliding) } -> std::same_as<void>;
		{ object.getBoundingBoxes() } -> std::same_as<std::vector<BoundingBox>&>;
	};

	template <Collidable T>
	class CollisionHandler {
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
			if ((box1.topCorner.x >= box2.bottomCorner.x && box1.bottomCorner.x <= box2.topCorner.x) ||
				(box1.topCorner.y >= box2.bottomCorner.y && box1.bottomCorner.y <= box2.topCorner.y) ||
				(box1.topCorner.z >= box2.bottomCorner.z && box1.bottomCorner.z <= box2.topCorner.z)) {
				setCollisionInformation(box1, box2);
				return true;
			}
			else {
				box1.collisionInformation = {};
				box2.collisionInformation = {};
				return false;
			}
		}

		static bool checkCollision(const glm::vec3& point, const BoundingBox& box) {
			return (point.x >= box.bottomCorner.x && point.x <= box.topCorner.x) &&
				   (point.y >= box.bottomCorner.y && point.y <= box.topCorner.y) &&  
				   (point.z >= box.bottomCorner.z && point.z <= box.topCorner.z);
		}

		static bool checkCollision(const std::vector<BoundingBox>& boxes1, const std::vector<BoundingBox>& boxes2) {
			for (auto& box1 : boxes1) {
				for (auto& box2 : boxes2) {
					if (checkCollision(box1, box2)) {
						return true;
					}
				}
			}
			return false;
		}

		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
			CollisionInformation collisionInformation;

			// Calculate the penetration
			glm::vec3 distance1 = box2.bottomCorner - box1.topCorner;
			glm::vec3 distance2 = box1.bottomCorner - box2.topCorner;
			auto penetration = glm::vec3(
				std::abs(distance1.x) < std::abs(distance2.x) ? distance1.x : distance2.x,
				std::abs(distance1.y) < std::abs(distance2.y) ? distance1.y : distance2.y,
				std::abs(distance1.z) < std::abs(distance2.z) ? distance1.z : distance2.z
			);

			// Calculate the collision normal
			glm::vec3 distance = glm::abs(box1.getCenter() - box2.getCenter());
			if (distance.x < distance.y && distance.x < distance.z) {
				collisionInformation.collisionNormal = glm::vec3(penetration.x, 0.0f, 0.0f);
			}
			else if (distance.y < distance.x && distance.y < distance.z) {
				collisionInformation.collisionNormal = glm::vec3(0.0f, penetration.y, 0.0f);
			}
			else {
				collisionInformation.collisionNormal = glm::vec3(0.0f, 0.0f, penetration.z);
			}

			collisionInformation.penetration = glm::length(penetration);
			collisionInformation.collisionPoint = box1.getCenter() + penetration / 2.0f;

			return collisionInformation;
		}

		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
			box1.collisionInformation = calculateCollisionInformation(box1, box2);
			box2.collisionInformation = calculateCollisionInformation(box2, box1);
		}

		void addObject(const T& object) {
			objects.push_back(object);
		}

		void removeObject(const T& object) {
			objects.erase(std::remove(objects.begin(), objects.end(), object), objects.end());
		}

		void update(float deltaTime) {
			for (auto& object : objects) {
				for (auto& other : objects) {
					if (object == other) continue;

					if (checkCollision(object.getBoundingBoxes(), other.getBoundingBoxes())) {
						object.setIsColliding(true);
						other.setIsColliding(true);
					}
					else {
						object.setIsColliding(false);
						other.setIsColliding(false);
					}
				}
			}
		}
		private:
			std::vector<T> objects;
	};
}
