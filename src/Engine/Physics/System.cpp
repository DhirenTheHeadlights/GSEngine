#include "Engine/Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Engine/Core/Clock.h"
#include "Engine/Physics/Vector/Math.h"

std::vector<Engine::Physics::MotionComponent*> Engine::Physics::components;

void Engine::Physics::applyForce(MotionComponent* component, const Vec3<Force>& force) {
	component->acceleration += 
		Engine::Vec3<Units::MetersPerSecondSquared>(
			force.as<Units::Newtons>() / 
			std::max(component->mass.as<Units::Kilograms>(), 0.0001f)
		);
}

void Engine::Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Engine::Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Engine::Physics::MotionComponent* component) {
	if (component->affectedByGravity && component->airborne) {
		const Engine::Vec3<Engine::Units::Newtons> gravity(
			0.f,
			-9.8f * component->mass.as<Engine::Units::Kilograms>(),
			0.f
		);
		applyForce(component, gravity);	
		component->acceleration.rawVec3().x *= 0.001f;
		component->acceleration.rawVec3().z *= 0.001f;
	}
	else {
		component->acceleration.rawVec3().y = std::max(0.f, component->acceleration.rawVec3().y);
	}
}

void updateAirResistance(Engine::Physics::MotionComponent* component) {
	// Constants
	constexpr float airDensity = 1.225f;								// kg/m^3 (air density at sea level)
	const float dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;							// Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const Engine::Force dragForceMagnitude(Engine::Units::Newtons(
		0.5f * dragCoefficient * airDensity * crossSectionalArea * 
		component->velocity.magnitude().as<Engine::Units::MetersPerSecond>() *
		component->velocity.magnitude().as<Engine::Units::MetersPerSecond>())
	); 

	// Apply the drag force to the object
	applyForce(component, Engine::Vec3<Engine::Units::Newtons>(-dragForceMagnitude.as<Engine::Units::Newtons>() * component->velocity.getDirection()));
}

void updatePosition(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();
	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->position += Engine::Vec3<Engine::Units::Meters>(
		component->velocity.as<Engine::Units::MetersPerSecond>() * deltaTime + 0.5f * 
		component->acceleration.as<Engine::Units::MetersPerSecondSquared>() * deltaTime * deltaTime);
}

void Engine::Physics::updateEntities() {
	for (MotionComponent* component : components) {
		updateGravity(component);
		updateAirResistance(component);
		updatePosition(component);
	}
}

void Engine::Physics::updateEntity(MotionComponent* component) {
	updateGravity(component);
	updateAirResistance(component);
	updatePosition(component);
}

void Engine::Physics::resolveCollision(BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, CollisionInformation& collisionInfo) {
	constexpr float epsilon = 0.0001f;

	// Check for collision on the bottom face (ground collision)
	if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().y, dynamicBoundingBox.lowerBound.rawVec3().y, epsilon)) {
		// Stop downward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().y = std::max(0.f, dynamicMotionComponent.velocity.rawVec3().y);
		dynamicMotionComponent.acceleration.rawVec3().y = std::max(0.f, dynamicMotionComponent.acceleration.rawVec3().y);
		dynamicMotionComponent.airborne = false;

		// Adjust position to be exactly on the ground
		dynamicMotionComponent.position.rawVec3().y = dynamicBoundingBox.lowerBound.rawVec3().y - (dynamicBoundingBox.lowerBound.rawVec3().y - dynamicMotionComponent.position.rawVec3().y);
	}
	// Check for collision on the top face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().y, dynamicBoundingBox.upperBound.rawVec3().y, epsilon)) {
		// Stop upward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().y = std::min(0.f, dynamicMotionComponent.velocity.rawVec3().y);
		dynamicMotionComponent.acceleration.rawVec3().y = std::min(0.f, dynamicMotionComponent.acceleration.rawVec3().y);

		// Adjust position to be just below the top face
		dynamicMotionComponent.position.rawVec3().y = dynamicBoundingBox.upperBound.rawVec3().y - (dynamicBoundingBox.upperBound.rawVec3().y - dynamicMotionComponent.position.rawVec3().y);
	}
	// Check for collision on the left face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().x, dynamicBoundingBox.lowerBound.rawVec3().x, epsilon)) {
		// Stop leftward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().x = std::max(0.f, dynamicMotionComponent.velocity.rawVec3().x);
		dynamicMotionComponent.acceleration.rawVec3().x = std::max(0.f, dynamicMotionComponent.acceleration.rawVec3().x);

		// Adjust position to be just to the right of the left face
		dynamicMotionComponent.position.rawVec3().x = dynamicBoundingBox.lowerBound.rawVec3().x - (dynamicBoundingBox.lowerBound.rawVec3().x - dynamicMotionComponent.position.rawVec3().x);
	}
	// Check for collision on the right face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().x, dynamicBoundingBox.upperBound.rawVec3().x, epsilon)) {
		// Stop rightward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().x = std::min(0.f, dynamicMotionComponent.velocity.rawVec3().x);
		dynamicMotionComponent.acceleration.rawVec3().x = std::min(0.f, dynamicMotionComponent.acceleration.rawVec3().x);

		// Adjust position to be just to the left of the right face
		dynamicMotionComponent.position.rawVec3().x = dynamicBoundingBox.upperBound.rawVec3().x - (dynamicBoundingBox.upperBound.rawVec3().x - dynamicMotionComponent.position.rawVec3().x);
	}
	// Check for collision on the back face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().z, dynamicBoundingBox.lowerBound.rawVec3().z, epsilon)) {
		// Stop backward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().z = std::max(0.f, dynamicMotionComponent.velocity.rawVec3().z);
		dynamicMotionComponent.acceleration.rawVec3().z = std::max(0.f, dynamicMotionComponent.acceleration.rawVec3().z);

		// Adjust position to be just in front of the back face
		dynamicMotionComponent.position.rawVec3().z = dynamicBoundingBox.lowerBound.rawVec3().z - (dynamicBoundingBox.lowerBound.rawVec3().z - dynamicMotionComponent.position.rawVec3().z);
	}
	// Check for collision on the front face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.rawVec3().z, dynamicBoundingBox.upperBound.rawVec3().z, epsilon)) {
		// Stop forward velocity and acceleration
		dynamicMotionComponent.velocity.rawVec3().z = std::min(0.f, dynamicMotionComponent.velocity.rawVec3().z);
		dynamicMotionComponent.acceleration.rawVec3().z = std::min(0.f, dynamicMotionComponent.acceleration.rawVec3().z);

		// Adjust position to be just behind the front face
		dynamicMotionComponent.position.rawVec3().z = dynamicBoundingBox.upperBound.rawVec3().z - (dynamicBoundingBox.upperBound.rawVec3().z - dynamicMotionComponent.position.rawVec3().z);
	}
	else {
		// Collision does not match any of the faces
	}
}

