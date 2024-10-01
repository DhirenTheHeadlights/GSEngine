#include "Engine/Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

using namespace Engine;

std::vector<Physics::MotionComponent*> Physics::components;

void Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Physics::MotionComponent* component, const float deltaTime) {
	if (component->affectedByGravity && component->airborne) {
		component->acceleration.y = -9.8f;
		component->acceleration.x *= 0.001f;
		component->acceleration.z *= 0.001f;
	}
	else {
		component->acceleration.y = std::max(0.f, component->acceleration.y);
	}
}

void updateAirResistance(Physics::MotionComponent* component, const float deltaTime) {
	const float dragCoefficient = component->airborne ? 0.2f : 0.9f;  // Lower drag in the air, higher on the ground

	const glm::vec3 velocityDragForce = component->velocity * -dragCoefficient;
	component->velocity += velocityDragForce * deltaTime;

	const glm::vec3 accelDragForce = component->acceleration * -dragCoefficient;
	component->acceleration += accelDragForce * deltaTime;
}

void updatePosition(Physics::MotionComponent* component, const float deltaTime) {
	component->velocity += component->acceleration * deltaTime;
	component->position += component->velocity * deltaTime;
}

void Physics::updateEntities(const float deltaTime) {
	for (MotionComponent* component : components) {
		// Update gravity only if the object is not airborne
		updateGravity(component, deltaTime);

		// Apply air resistance (can be applied even when airborne to simulate drag)
		updateAirResistance(component, deltaTime);

		// Update velocity and position
		updatePosition(component, deltaTime);
	}
}

void Physics::resolveCollision(const BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	if (glm::epsilonEqual(collisionInfo.collisionPoint.y, dynamicBoundingBox.lowerBound.y, 0.0001f)) {
		// Ground collision, stop downward velocity and mark the object as airborne
		dynamicMotionComponent.velocity.y = std::max(0.f, dynamicMotionComponent.velocity.y);
		dynamicMotionComponent.acceleration.y = std::max(0.f, dynamicMotionComponent.acceleration.y);
		dynamicMotionComponent.airborne = false;

		// Set the position so that the bounding box is exactly on the ground,
		// accounting for the difference in position between the motion component
		// and the bounding box
		dynamicMotionComponent.position.y = std::max(dynamicBoundingBox.lowerBound.y - (dynamicBoundingBox.lowerBound.y - dynamicMotionComponent.position.y), dynamicBoundingBox.lowerBound.y);
	}
	else {
	}
}
