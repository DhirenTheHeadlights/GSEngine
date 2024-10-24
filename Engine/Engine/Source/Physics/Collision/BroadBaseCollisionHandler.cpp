#include "Physics/Collision/BroadPhaseCollisionHandler.h"

#include <iostream>

#include "Physics/System.h"
#include "Physics/Vector/Math.h"

bool Engine::BroadPhaseCollisionHandler::checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
	return box1.upperBound.rawVec3().x > box2.lowerBound.rawVec3().x && box1.lowerBound.rawVec3().x < box2.upperBound.rawVec3().x &&
		   box1.upperBound.rawVec3().y > box2.lowerBound.rawVec3().y && box1.lowerBound.rawVec3().y < box2.upperBound.rawVec3().y &&
		   box1.upperBound.rawVec3().z > box2.lowerBound.rawVec3().z && box1.lowerBound.rawVec3().z < box2.upperBound.rawVec3().z;
}

bool Engine::BroadPhaseCollisionHandler::checkCollision(const BoundingBox& dynamicBox, const BoundingBox& staticBox, const std::shared_ptr<Physics::MotionComponent>& component) {
	BoundingBox expandedBox = dynamicBox;					// Create a copy
	Physics::MotionComponent tempComponent = *component;    
	updateEntity(&tempComponent);							// Update the entity's position in the direction of its velocity
	expandedBox.setPosition(tempComponent.position);		// Set the expanded box's position to the updated position
	return checkCollision(expandedBox, staticBox);			// Check for collision with the new expanded box
}

bool Engine::BroadPhaseCollisionHandler::checkCollision(const Vec3<Length>& point, const BoundingBox& box) {
	return point.rawVec3().x > box.lowerBound.rawVec3().x && point.rawVec3().x < box.upperBound.rawVec3().x &&
		   point.rawVec3().y > box.lowerBound.rawVec3().y && point.rawVec3().y < box.upperBound.rawVec3().y &&
		   point.rawVec3().z > box.lowerBound.rawVec3().z && point.rawVec3().z < box.upperBound.rawVec3().z;
} 

bool Engine::BroadPhaseCollisionHandler::checkCollision(const std::shared_ptr<DynamicObject>& object1, const std::shared_ptr<Object>& object2) {
	for (auto& box1 : object1->getBoundingBoxes()) {
		for (auto& box2 : object2->getBoundingBoxes()) {
			if (checkCollision(box1, box2, object1->getMotionComponent())) {
				setCollisionInformation(box1, box2);

				box1.collisionInformation.colliding = true;
				box2.collisionInformation.colliding = true;

				resolveCollision(box1, object1->getMotionComponent(), box2.collisionInformation);

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
	Length xPenetration = min(box1.upperBound, box2.upperBound, 0) - max(box1.lowerBound, box2.lowerBound, 0);
	Length yPenetration = min(box1.upperBound, box2.upperBound, 1) - max(box1.lowerBound, box2.lowerBound, 1);
	Length zPenetration = min(box1.upperBound, box2.upperBound, 2) - max(box1.lowerBound, box2.lowerBound, 2);

	// Find the axis with the smallest penetration
	Length penetration = xPenetration;
	Vec3<Unitless> collisionNormal(1.0f, 0.0f, 0.0f); // Default to X axis

	if (yPenetration < penetration) {
		penetration = yPenetration;
		collisionNormal = Vec3<Unitless>(0.0f, 1.0f, 0.0f);
	}
	if (zPenetration < penetration) {
		penetration = zPenetration;
		collisionNormal = Vec3<Unitless>(0.0f, 0.0f, 1.0f);
	}

	// Determine the rawVec3 of the collision normal
	const Vec3<Length> deltaCenter = box2.getCenter() - box1.getCenter();
	if (dot(deltaCenter, collisionNormal) < 0.0f) {
		collisionNormal = -collisionNormal;
	}

	// Calculate the collision point
	Vec3<Units::Meters> collisionPoint = box1.getCenter();
	collisionPoint += box1.getSize() / 2.f * collisionNormal;															// Move to the surface of box1
	collisionPoint -= Vec3<Units::Meters>(penetration * collisionNormal);

	collisionInformation.collisionNormal = collisionNormal;
	collisionInformation.penetration = penetration;
	collisionInformation.collisionPoint = collisionPoint;

	return collisionInformation;
}

void Engine::BroadPhaseCollisionHandler::update() const {
	for (auto& dynamicObject : dynamicObjects) {
		const auto dynamicObjectPtr = dynamicObject.lock();
		if (!dynamicObjectPtr) {
			continue;
		}

		for (auto& object : objects) {
			const auto objectPtr = object.lock();
			if (!objectPtr) {
				continue;
			}

			if (dynamicObjectPtr == objectPtr) {
				continue;
			}

			if (checkCollision(dynamicObjectPtr, objectPtr)) {
				dynamicObjectPtr->setIsColliding(true);
				objectPtr->setIsColliding(true);
			}
			else {
				dynamicObjectPtr->getMotionComponent()->airborne = true;
				objectPtr->setIsColliding(false);
				dynamicObjectPtr->setIsColliding(false);
			}
		}
	}
}

