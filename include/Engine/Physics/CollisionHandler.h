#pragma once

#include <vector>
#include <concepts>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "BoundingBox.h"

namespace Engine {
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
			if ((box1.upperBound.x >= box2.lowerBound.x && box1.lowerBound.x <= box2.upperBound.x) ||
				(box1.upperBound.y >= box2.lowerBound.y && box1.lowerBound.y <= box2.upperBound.y) ||
				(box1.upperBound.z >= box2.lowerBound.z && box1.lowerBound.z <= box2.upperBound.z)) {
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
			return (point.x >= box.lowerBound.x && point.x <= box.upperBound.x) &&
				   (point.y >= box.lowerBound.y && point.y <= box.upperBound.y) &&  
				   (point.z >= box.lowerBound.z && point.z <= box.upperBound.z);
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
			glm::vec3 distance1 = box2.lowerBound - box1.upperBound;
			glm::vec3 distance2 = box1.lowerBound - box2.upperBound;
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
