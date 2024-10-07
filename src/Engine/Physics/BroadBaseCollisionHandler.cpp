#include "Engine/Physics/BroadPhaseCollisionHandler.h"

#include <iostream>

#include "Engine/Physics/System.h"

bool Engine::BroadPhaseCollisionHandler::checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
	return (box1.upperBound.x >= box2.lowerBound.x && box1.lowerBound.x <= box2.upperBound.x) &&
		   (box1.upperBound.y >= box2.lowerBound.y && box1.lowerBound.y <= box2.upperBound.y) &&
		   (box1.upperBound.z >= box2.lowerBound.z && box1.lowerBound.z <= box2.upperBound.z);
}

bool Engine::BroadPhaseCollisionHandler::checkCollision(const BoundingBox& dynamicBox, const BoundingBox& staticBox, const glm::vec3& totalVelocity) {
	BoundingBox expandedBox = dynamicBox;

	// Expand the bounding box in the direction of velocity
	expandedBox.lowerBound = min(dynamicBox.lowerBound, dynamicBox.lowerBound + totalVelocity);
	expandedBox.upperBound = max(dynamicBox.upperBound, dynamicBox.upperBound + totalVelocity);

	return checkCollision(expandedBox, staticBox);
}

bool Engine::BroadPhaseCollisionHandler::checkCollision(const glm::vec3& point, const BoundingBox& box) {
	return (point.x >= box.lowerBound.x && point.x <= box.upperBound.x) &&
		   (point.y >= box.lowerBound.y && point.y <= box.upperBound.y) &&
		   (point.z >= box.lowerBound.z && point.z <= box.upperBound.z);
}

bool Engine::BroadPhaseCollisionHandler::checkCollision(DynamicObject& object1, Object& object2) {
	for (auto& box1 : object1.getBoundingBoxes()) {
		for (auto& box2 : object2.getBoundingBoxes()) {
			if (checkCollision(box1, box2, object1.getMotionComponent().velocity.as<Units::MetersPerSecond>())) {
				setCollisionInformation(box1, box2);

				box1.collisionInformation.colliding = true;
				box2.collisionInformation.colliding = true;

				resolveCollision(box1, object1.getMotionComponent(), box2.collisionInformation);

				return true;
			}

			box1.collisionInformation = {};
			box2.collisionInformation = {};
		}
	}
	return false;
}

void Engine::BroadPhaseCollisionHandler::setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	box1.collisionInformation = calculateCollisionInformation(box1, box2);
	box2.collisionInformation = calculateCollisionInformation(box2, box1);
}

Engine::CollisionInformation Engine::BroadPhaseCollisionHandler::calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	CollisionInformation collisionInformation;

	if (!checkCollision(box1, box2)) {
		return collisionInformation;
	}

	// Calculate the penetration depth on each axis
	float xPenetration = std::min(box1.upperBound.x, box2.upperBound.x) - std::max(box1.lowerBound.x, box2.lowerBound.x);
	float yPenetration = std::min(box1.upperBound.y, box2.upperBound.y) - std::max(box1.lowerBound.y, box2.lowerBound.y);
	float zPenetration = std::min(box1.upperBound.z, box2.upperBound.z) - std::max(box1.lowerBound.z, box2.lowerBound.z);

	// Find the axis with the smallest penetration
	float penetration = xPenetration;
	auto collisionNormal = glm::vec3(1.0f, 0.0f, 0.0f); // Default to X axis

	if (yPenetration < penetration) {
		penetration = yPenetration;
		collisionNormal = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	if (zPenetration < penetration) {
		penetration = zPenetration;
		collisionNormal = glm::vec3(0.0f, 0.0f, 1.0f);
	}

	// Determine the direction of the collision normal
	glm::vec3 deltaCenter = box2.getCenter() - box1.getCenter();
	if (dot(deltaCenter, collisionNormal) < 0.0f) {
		collisionNormal = -collisionNormal;
	}

	// Calculate the collision point
	glm::vec3 collisionPoint = box1.getCenter();
	collisionPoint += box1.getSize() / 2.f * collisionNormal;			// Move to the surface of box1
	collisionPoint -= collisionNormal * penetration;					// Adjust by half the penetration depth

	collisionInformation.collisionNormal = collisionNormal;
	collisionInformation.penetration = penetration;
	collisionInformation.collisionPoint = collisionPoint;

	return collisionInformation;
}

void Engine::BroadPhaseCollisionHandler::update() const {
	// Check for collisions
	for (auto& dynamicObjectPtr : dynamicObjects) {
		for (auto& objectPtr : objects) {
			if (checkCollision(*dynamicObjectPtr, *objectPtr)) {
				dynamicObjectPtr->setIsColliding(true);
				objectPtr->setIsColliding(true);
			}
			else {
				dynamicObjectPtr->getMotionComponent().airborne = true;

				objectPtr->setIsColliding(false);
				dynamicObjectPtr->setIsColliding(false);
			}
		}
	}
}
