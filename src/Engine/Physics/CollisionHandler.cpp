#include "Engine/Physics/CollisionHandler.h"

using namespace Engine;

bool CollisionHandler::checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
	if ((box1.upperBound.x >= box2.lowerBound.x && box1.lowerBound.x <= box2.upperBound.x) &&
		(box1.upperBound.y >= box2.lowerBound.y && box1.lowerBound.y <= box2.upperBound.y) &&
		(box1.upperBound.z >= box2.lowerBound.z && box1.lowerBound.z <= box2.upperBound.z)) {
		setCollisionInformation(box1, box2);
		return true;
	}

	box1.collisionInformation = {};
	box2.collisionInformation = {};
	return false;
}

bool CollisionHandler::checkCollision(const glm::vec3 & point, const BoundingBox & box) {
	return (point.x >= box.lowerBound.x && point.x <= box.upperBound.x) &&
		(point.y >= box.lowerBound.y && point.y <= box.upperBound.y) &&
		(point.z >= box.lowerBound.z && point.z <= box.upperBound.z);
}

bool CollisionHandler::checkCollision(const std::vector<BoundingBox>& boxes1, const std::vector<BoundingBox>& boxes2) {
	for (auto& box1 : boxes1) {
		for (auto& box2 : boxes2) {
			if (checkCollision(box1, box2)) {
				return true;
			}
		}
	}
	return false;
}

CollisionInformation CollisionHandler::calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	CollisionInformation collisionInformation{};

	// Calculate the penetration depth on each axis
	float xPenetration = std::min(box1.upperBound.x, box2.upperBound.x) - std::max(box1.lowerBound.x, box2.lowerBound.x);
	float yPenetration = std::min(box1.upperBound.y, box2.upperBound.y) - std::max(box1.lowerBound.y, box2.lowerBound.y);
	float zPenetration = std::min(box1.upperBound.z, box2.upperBound.z) - std::max(box1.lowerBound.z, box2.lowerBound.z);

	// Find the axis with the smallest penetration
	const float penetration = std::min({ xPenetration, yPenetration, zPenetration });

	// Set the collision normal based on the axis of least penetration
	if (glm::epsilonEqual(penetration, xPenetration, 0.0001f)) {
		collisionInformation.collisionNormal = glm::vec3(xPenetration, 0.0f, 0.0f);
	}
	else if (glm::epsilonEqual(penetration, yPenetration, 0.0001f)) {
		collisionInformation.collisionNormal = glm::vec3(0.0f, yPenetration, 0.0f);
	}
	else {
		collisionInformation.collisionNormal = glm::vec3(0.0f, 0.0f, zPenetration);
	}

	collisionInformation.penetration = penetration;
	collisionInformation.collisionPoint = (box1.getCenter() + box2.getCenter()) / 2.0f;

	return collisionInformation;
}

void CollisionHandler::setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	box1.collisionInformation = calculateCollisionInformation(box1, box2);
	box2.collisionInformation = calculateCollisionInformation(box2, box1);
}

void CollisionHandler::update() const {
	// Reset collision state for all objects
	for (auto& objectPtr : objects) {
		Object& object = *objectPtr;
		object.setIsColliding(false);
	}

	// Check for collisions
	for (size_t i = 0; i < objects.size(); ++i) {
		Object& object = *objects[i];
		for (size_t j = i + 1; j < objects.size(); ++j) {
			if (Object& other = *objects[j]; checkCollision(object.getBoundingBoxes(), other.getBoundingBoxes())) {
				object.setIsColliding(true);
				other.setIsColliding(true);
			}
		}
	}
}
