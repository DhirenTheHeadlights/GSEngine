#include "Engine/Physics/BroadPhaseCollisionHandler.h"

#include <iostream>

#include "Engine/Physics/System.h"

using namespace Engine;

bool BroadPhaseCollisionHandler::checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
	return ((box1.upperBound.x >= box2.lowerBound.x && box1.lowerBound.x <= box2.upperBound.x) &&
			(box1.upperBound.y >= box2.lowerBound.y && box1.lowerBound.y <= box2.upperBound.y) &&
			(box1.upperBound.z >= box2.lowerBound.z && box1.lowerBound.z <= box2.upperBound.z));
}

bool BroadPhaseCollisionHandler::checkCollision(const BoundingBox& dynamicBox, const BoundingBox& staticBox, const glm::vec3& totalVelocity) {
	// Check if the dynamic box will collide with the static box after moving
	BoundingBox tempBox = dynamicBox;

	tempBox.move(totalVelocity);

	return checkCollision(tempBox, staticBox);
}

bool BroadPhaseCollisionHandler::checkCollision(const glm::vec3& point, const BoundingBox& box) {
	return (point.x >= box.lowerBound.x && point.x <= box.upperBound.x) &&
		   (point.y >= box.lowerBound.y && point.y <= box.upperBound.y) &&
		   (point.z >= box.lowerBound.z && point.z <= box.upperBound.z);
}

bool BroadPhaseCollisionHandler::checkCollision(DynamicObject& object1, Object& object2) {
	for (auto& box1 : object1.getBoundingBoxes()) {
		for (auto& box2 : object2.getBoundingBoxes()) {
			if (checkCollision(box1, box2, object1.getMotionComponent().velocity)) {
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

CollisionInformation BroadPhaseCollisionHandler::calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
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

void BroadPhaseCollisionHandler::setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	box1.collisionInformation = calculateCollisionInformation(box1, box2);
	box2.collisionInformation = calculateCollisionInformation(box2, box1);
}

void BroadPhaseCollisionHandler::update() const {
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
