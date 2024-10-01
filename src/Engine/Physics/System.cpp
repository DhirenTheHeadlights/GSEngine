#include "Engine/Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

std::vector<Engine::Physics::MotionComponent*> Engine::Physics::components;

void Engine::Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Engine::Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Engine::Physics::MotionComponent* component, const float deltaTime) {
	if (component->affectedByGravity && component->airborne) {
		component->acceleration.y = -9.8f;
		component->acceleration.x *= 0.001f;
		component->acceleration.z *= 0.001f;
	}
	else {
		component->acceleration.y = std::max(0.f, component->acceleration.y);
	}
}

void updateAirResistance(Engine::Physics::MotionComponent* component, const float deltaTime) {
	const float dragCoefficient = component->airborne ? 0.2f : 0.9f;  // Lower drag in the air, higher on the ground

	const glm::vec3 velocityDragForce = component->velocity * -dragCoefficient;
	component->velocity += velocityDragForce * deltaTime;

	const glm::vec3 accelDragForce = component->acceleration * -dragCoefficient;
	component->acceleration += accelDragForce * deltaTime;
}

void updatePosition(Engine::Physics::MotionComponent* component, const float deltaTime) {
	component->velocity += component->acceleration * deltaTime;
	component->position += component->velocity * deltaTime;
}

void Engine::Physics::updateEntities(const float deltaTime) {
	for (MotionComponent* component : components) {
		// Update gravity only if the object is not airborne
		updateGravity(component, deltaTime);

		// Apply air resistance (can be applied even when airborne to simulate drag)
		updateAirResistance(component, deltaTime);

		// Update velocity and position
		updatePosition(component, deltaTime);
	}
}

void Engine::Physics::resolveCollision(const BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	constexpr float epsilon = 0.0001f;

	// Check for collision on the bottom face (ground collision)
	if (glm::epsilonEqual(collisionInfo.collisionPoint.y, dynamicBoundingBox.lowerBound.y, epsilon)) {
		// Stop downward velocity and acceleration
		dynamicMotionComponent.velocity.y = std::max(0.f, dynamicMotionComponent.velocity.y);
		dynamicMotionComponent.acceleration.y = std::max(0.f, dynamicMotionComponent.acceleration.y);
		dynamicMotionComponent.airborne = false;

		// Adjust position to be exactly on the ground
		dynamicMotionComponent.position.y = dynamicBoundingBox.lowerBound.y - (dynamicBoundingBox.lowerBound.y - dynamicMotionComponent.position.y);
	}
	// Check for collision on the top face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.y, dynamicBoundingBox.upperBound.y, epsilon)) {
		// Stop upward velocity and acceleration
		dynamicMotionComponent.velocity.y = std::min(0.f, dynamicMotionComponent.velocity.y);
		dynamicMotionComponent.acceleration.y = std::min(0.f, dynamicMotionComponent.acceleration.y);

		// Adjust position to be just below the top face
		dynamicMotionComponent.position.y = dynamicBoundingBox.upperBound.y - (dynamicBoundingBox.upperBound.y - dynamicMotionComponent.position.y);
	}
	// Check for collision on the left face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.x, dynamicBoundingBox.lowerBound.x, epsilon)) {
		// Stop leftward velocity and acceleration
		dynamicMotionComponent.velocity.x = std::max(0.f, dynamicMotionComponent.velocity.x);
		dynamicMotionComponent.acceleration.x = std::max(0.f, dynamicMotionComponent.acceleration.x);

		// Adjust position to be just to the right of the left face
		dynamicMotionComponent.position.x = dynamicBoundingBox.lowerBound.x - (dynamicBoundingBox.lowerBound.x - dynamicMotionComponent.position.x);
	}
	// Check for collision on the right face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.x, dynamicBoundingBox.upperBound.x, epsilon)) {
		// Stop rightward velocity and acceleration
		dynamicMotionComponent.velocity.x = std::min(0.f, dynamicMotionComponent.velocity.x);
		dynamicMotionComponent.acceleration.x = std::min(0.f, dynamicMotionComponent.acceleration.x);

		// Adjust position to be just to the left of the right face
		dynamicMotionComponent.position.x = dynamicBoundingBox.upperBound.x - (dynamicBoundingBox.upperBound.x - dynamicMotionComponent.position.x);
	}
	// Check for collision on the back face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.z, dynamicBoundingBox.lowerBound.z, epsilon)) {
		// Stop backward velocity and acceleration
		dynamicMotionComponent.velocity.z = std::max(0.f, dynamicMotionComponent.velocity.z);
		dynamicMotionComponent.acceleration.z = std::max(0.f, dynamicMotionComponent.acceleration.z);

		// Adjust position to be just in front of the back face
		dynamicMotionComponent.position.z = dynamicBoundingBox.lowerBound.z - (dynamicBoundingBox.lowerBound.z - dynamicMotionComponent.position.z);
	}
	// Check for collision on the front face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.z, dynamicBoundingBox.upperBound.z, epsilon)) {
		// Stop forward velocity and acceleration
		dynamicMotionComponent.velocity.z = std::min(0.f, dynamicMotionComponent.velocity.z);
		dynamicMotionComponent.acceleration.z = std::min(0.f, dynamicMotionComponent.acceleration.z);

		// Adjust position to be just behind the front face
		dynamicMotionComponent.position.z = dynamicBoundingBox.upperBound.z - (dynamicBoundingBox.upperBound.z - dynamicMotionComponent.position.z);
	}
	else {
		// Collision does not match any of the faces
	}
}

