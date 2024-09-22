#include "Engine/Physics/System.h"
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>  // For glm::length2

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
	}
	else {
		component->acceleration.y = 0.0f;
	}
}

void updateAirResistance(Physics::MotionComponent* component, const float deltaTime) {
	const glm::vec3 dragForce = component->velocity * -0.1f;  // 0.1 is an arbitrary drag coefficient
	component->acceleration += dragForce * deltaTime;
}

void updatePosition(Physics::MotionComponent* component, const float deltaTime) {
	component->velocity += component->acceleration * deltaTime;

	// Prevent negative velocity when decelerating
	if (glm::length2(component->velocity) < 0.0001f) {  // length2 is faster as it skips square root
		component->velocity = glm::vec3(0.0f);
	}

	// Update position using velocity
	component->position += component->velocity * deltaTime;
}

void Physics::updateEntities(const float deltaTime) {
	for (MotionComponent* component : components) {
		// Update gravity only if the object is not airborne
		//updateGravity(component, deltaTime);

		// Apply air resistance (can be applied even when airborne to simulate drag)
		updateAirResistance(component, deltaTime);

		// Update velocity and position
		updatePosition(component, deltaTime);
	}
}

void Physics::resolveCollision(MotionComponent& component, const CollisionInformation& collisionInfo) {
	if (glm::epsilonEqual(collisionInfo.collisionPoint.y, component.position.y, 0.0001f)) {
		// Ground collision, stop downward velocity and mark the object as airborne
		component.velocity.y = 0;
		component.acceleration.y = 0;
		component.airborne = false;
		component.position.y = collisionInfo.collisionPoint.y;  // Snap to the collision point
	}
	else {
	}
}
