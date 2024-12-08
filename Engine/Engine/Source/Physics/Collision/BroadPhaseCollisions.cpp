#include "Physics/Collision/BroadPhaseCollisions.h"

#include <iostream>

#include "Physics/System.h"
#include "Physics/Vector/Math.h"

void Engine::BroadPhaseCollision::Group::addDynamicObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent, const std::shared_ptr<Physics::MotionComponent>& motionComponent) {
	dynamicObjects.emplace_back(collisionComponent, motionComponent);
}

void Engine::BroadPhaseCollision::Group::addObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent) {
	objects.emplace_back(collisionComponent);
}

void Engine::BroadPhaseCollision::Group::removeObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent) {
	std::erase_if(objects, [&](const Object& obj) {
		return !obj.collisionComponent.owner_before(collisionComponent) && !collisionComponent.owner_before(obj.collisionComponent);
		});

	std::erase_if(dynamicObjects, [&](const DynamicObject& obj) {
		return !obj.collisionComponent.owner_before(collisionComponent) && !collisionComponent.owner_before(obj.collisionComponent);
		});
}

bool Engine::BroadPhaseCollision::checkCollision(const BoundingBox& box1, const BoundingBox& box2) {
	return box1.upperBound.asDefaultUnits().x > box2.lowerBound.asDefaultUnits().x && box1.lowerBound.asDefaultUnits().x < box2.upperBound.asDefaultUnits().x &&
		   box1.upperBound.asDefaultUnits().y > box2.lowerBound.asDefaultUnits().y && box1.lowerBound.asDefaultUnits().y < box2.upperBound.asDefaultUnits().y &&
		   box1.upperBound.asDefaultUnits().z > box2.lowerBound.asDefaultUnits().z && box1.lowerBound.asDefaultUnits().z < box2.upperBound.asDefaultUnits().z;
}

bool Engine::BroadPhaseCollision::checkCollision(const BoundingBox& dynamicBox, const std::shared_ptr<Physics::MotionComponent>& dynamicMotionComponent, const BoundingBox& otherBox) {
	BoundingBox expandedBox = dynamicBox;										// Create a copy
	Physics::MotionComponent tempComponent = *dynamicMotionComponent;
	updateEntity(&tempComponent);												// Update the entity's position in the direction of its velocity
	expandedBox.setPosition(tempComponent.position);							// Set the expanded box's position to the updated position
	return checkCollision(expandedBox, otherBox);								// Check for collision with the new expanded box
}

bool Engine::BroadPhaseCollision::checkCollision(const Vec3<Length>& point, const BoundingBox& box) {
	return point.asDefaultUnits().x > box.lowerBound.asDefaultUnits().x && point.asDefaultUnits().x < box.upperBound.asDefaultUnits().x &&
		   point.asDefaultUnits().y > box.lowerBound.asDefaultUnits().y && point.asDefaultUnits().y < box.upperBound.asDefaultUnits().y &&
		   point.asDefaultUnits().z > box.lowerBound.asDefaultUnits().z && point.asDefaultUnits().z < box.upperBound.asDefaultUnits().z;
} 

bool Engine::BroadPhaseCollision::checkCollision(const std::shared_ptr<Physics::CollisionComponent>& dynamicObjectCollisionComponent, const std::shared_ptr<Physics::MotionComponent>& dynamicObjectMotionComponent, const std::shared_ptr<Physics::CollisionComponent>& otherCollisionComponent) {
	for (auto& box1 : dynamicObjectCollisionComponent->boundingBoxes) {
		for (auto& box2 : otherCollisionComponent->boundingBoxes) {
			if (checkCollision(box1, box2)) {
				setCollisionInformation(box1, box2);

				box1.collisionInformation.colliding = true;
				box2.collisionInformation.colliding = true;

				resolveCollision(box1, dynamicObjectMotionComponent, box2.collisionInformation);

				return true;
			}

			box1.collisionInformation = {};
			box2.collisionInformation = {};
		}
	}
	return false;
}

Engine::CollisionInformation Engine::BroadPhaseCollision::calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	CollisionInformation collisionInformation;

	if (!checkCollision(box1, box2)) {
		return collisionInformation;
	}

	// Calculate the penetration depth on each axis
	Length xPenetration = min(box1.upperBound, box2.upperBound, X) - max(box1.lowerBound, box2.lowerBound, X);
	Length yPenetration = min(box1.upperBound, box2.upperBound, Y) - max(box1.lowerBound, box2.lowerBound, Y);
	Length zPenetration = min(box1.upperBound, box2.upperBound, Z) - max(box1.lowerBound, box2.lowerBound, Z);

	// Find the axis with the smallest penetration
	Length penetration = xPenetration;
	Vec3 collisionNormal(1.0f, 0.0f, 0.0f); // Default to X axis

	if (yPenetration < penetration) {
		penetration = yPenetration;
		collisionNormal = Vec3(0.0f, 1.0f, 0.0f);
	}
	if (zPenetration < penetration) {
		penetration = zPenetration;
		collisionNormal = Vec3(0.0f, 0.0f, 1.0f);
	}

	// Determine the collision normal
	const Vec3<Length> deltaCenter = box2.getCenter() - box1.getCenter();
	if (dot(deltaCenter, collisionNormal) < 0.0f) {
		collisionNormal = -collisionNormal;
	}

	// Calculate the collision point
	Vec3<Meters> collisionPoint = box1.getCenter();
	collisionPoint += box1.getSize() / 2.f * collisionNormal;					
	collisionPoint -= penetration * collisionNormal;

	collisionInformation.collisionNormal = collisionNormal;
	collisionInformation.penetration = penetration;
	collisionInformation.collisionPoint = collisionPoint;

	return collisionInformation;
}

void Engine::BroadPhaseCollision::setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2) {
	box1.collisionInformation = calculateCollisionInformation(box1, box2);
	box2.collisionInformation = calculateCollisionInformation(box2, box1);
}

void Engine::BroadPhaseCollision::update(BroadPhaseCollision::Group& group) {
	for (auto& dynamicObject : group.getDynamicObjects()) {
		const auto dynamicObjectPtr = dynamicObject.collisionComponent.lock();
		if (!dynamicObjectPtr) {
			continue;
		}

		const auto motionComponentPtr = dynamicObject.motionComponent.lock();
		if (!motionComponentPtr) {
			continue;
		}

		for (auto& object : group.getObjects()) {
			const auto objectPtr = object.collisionComponent.lock();
			if (!objectPtr) {
				continue;
			}

			if (dynamicObjectPtr == objectPtr) {
				continue;
			}

			if (!checkCollision(dynamicObjectPtr, motionComponentPtr, objectPtr)) {
				motionComponentPtr->airborne = true;
			}
		}
	}
}

