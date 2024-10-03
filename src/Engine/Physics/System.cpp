#include "Engine/Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Engine/Core/Clock.h"

std::vector<Engine::Physics::MotionComponent*> Engine::Physics::components;

void Engine::Physics::applyForce(MotionComponent* component, const Vec3<Force>& force) {
	component->acceleration += force.as<Units::Newtons> / std::max(component->mass.as<Units::Kilograms>(), 0.0001f) * MainClock::getDeltaTime().as<Units::Seconds>();
}

void Engine::Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Engine::Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Engine::Physics::MotionComponent* component) {
	if (component->affectedByGravity && component->airborne) {
		Engine::Vec3<Engine::Force> gravity(Engine::Vec3Units<Engine::Units::Newtons>(glm::vec3(0, -9.81f, 0) * component->mass.as<Engine::Units::Kilograms>()));
		applyForce(component, gravity);
		component->acceleration.getFullVector().x *= 0.001f;
		component->acceleration.getFullVector().z *= 0.001f;
	}
	else {
		component->acceleration.getFullVector().y = std::max(0.f, component->acceleration.getFullVector().y);
	}
}

void updateAirResistance(Engine::Physics::MotionComponent* component) {
	// Constants
	constexpr float airDensity = 1.225f;  // kg/m^3 (air density at sea level)
	const float dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;  // Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2
	const Engine::Force dragForceMagnitude(0.5f * dragCoefficient * airDensity * crossSectionalArea * component->velocity.length().as<Engine::Units::MetersPerSecond>() * component->velocity.length().as<Engine::Units::MetersPerSecond>());

	// Apply the drag force to the object
	applyForce(component, Engine::Vec3<Engine::Force>(-dragForceMagnitude.as<Engine::Units::Newtons>() * component->velocity.getDirection()));
}

void updatePosition(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();
	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->velocity += component->velocity.length().as<Engine::Units::MetersPerSecond>() * deltaTime + 0.5f * 
						   component->acceleration.length().as<Engine::Units::MetersPerSecondSquared>() * deltaTime * deltaTime;
}

void Engine::Physics::updateEntities() {
	for (MotionComponent* component : components) {
		// Update gravity only if the object is not airborne
		updateGravity(component);

		// Apply air resistance (can be applied even when airborne to simulate drag)
		updateAirResistance(component);

		// Update velocity and position
		updatePosition(component);
	}
}

void Engine::Physics::resolveCollision(const BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	constexpr float epsilon = 0.0001f;

	// Check for collision on the bottom face (ground collision)
	if (glm::epsilonEqual(collisionInfo.collisionPoint.y, dynamicBoundingBox.lowerBound.y, epsilon)) {
		// Stop downward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().y = std::max(0.f, dynamicMotionComponent.velocity.getFullVector().y);
		dynamicMotionComponent.acceleration.getFullVector().y = std::max(0.f, dynamicMotionComponent.acceleration.getFullVector().y);
		dynamicMotionComponent.airborne = false;

		// Adjust position to be exactly on the ground
		dynamicMotionComponent.position.getFullVector().y = dynamicBoundingBox.lowerBound.y - (dynamicBoundingBox.lowerBound.y - dynamicMotionComponent.position.getFullVector().y);
	}
	// Check for collision on the top face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.y, dynamicBoundingBox.upperBound.y, epsilon)) {
		// Stop upward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().y = std::min(0.f, dynamicMotionComponent.velocity.getFullVector().y);
		dynamicMotionComponent.acceleration.getFullVector().y = std::min(0.f, dynamicMotionComponent.acceleration.getFullVector().y);

		// Adjust position to be just below the top face
		dynamicMotionComponent.position.getFullVector().y = dynamicBoundingBox.upperBound.y - (dynamicBoundingBox.upperBound.y - dynamicMotionComponent.position.getFullVector().y);
	}
	// Check for collision on the left face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.x, dynamicBoundingBox.lowerBound.x, epsilon)) {
		// Stop leftward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().x = std::max(0.f, dynamicMotionComponent.velocity.getFullVector().x);
		dynamicMotionComponent.acceleration.getFullVector().x = std::max(0.f, dynamicMotionComponent.acceleration.getFullVector().x);

		// Adjust position to be just to the right of the left face
		dynamicMotionComponent.position.getFullVector().x = dynamicBoundingBox.lowerBound.x - (dynamicBoundingBox.lowerBound.x - dynamicMotionComponent.position.getFullVector().x);
	}
	// Check for collision on the right face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.x, dynamicBoundingBox.upperBound.x, epsilon)) {
		// Stop rightward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().x = std::min(0.f, dynamicMotionComponent.velocity.getFullVector().x);
		dynamicMotionComponent.acceleration.getFullVector().x = std::min(0.f, dynamicMotionComponent.acceleration.getFullVector().x);

		// Adjust position to be just to the left of the right face
		dynamicMotionComponent.position.getFullVector().x = dynamicBoundingBox.upperBound.x - (dynamicBoundingBox.upperBound.x - dynamicMotionComponent.position.getFullVector().x);
	}
	// Check for collision on the back face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.z, dynamicBoundingBox.lowerBound.z, epsilon)) {
		// Stop backward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().z = std::max(0.f, dynamicMotionComponent.velocity.z);
		dynamicMotionComponent.acceleration.getFullVector().z = std::max(0.f, dynamicMotionComponent.acceleration.z);

		// Adjust position to be just in front of the back face
		dynamicMotionComponent.position.getFullVector().z = dynamicBoundingBox.lowerBound.z - (dynamicBoundingBox.lowerBound.z - dynamicMotionComponent.position.getFullVector().z);
	}
	// Check for collision on the front face
	else if (glm::epsilonEqual(collisionInfo.collisionPoint.z, dynamicBoundingBox.upperBound.z, epsilon)) {
		// Stop forward velocity and acceleration
		dynamicMotionComponent.velocity.getFullVector().z = std::min(0.f, dynamicMotionComponent.velocity.getFullVector().z);
		dynamicMotionComponent.acceleration.getFullVector().z = std::min(0.f, dynamicMotionComponent.acceleration.getFullVector().z);

		// Adjust position to be just behind the front face
		dynamicMotionComponent.position.getFullVector().z = dynamicBoundingBox.upperBound.z - (dynamicBoundingBox.upperBound.z - dynamicMotionComponent.position.getFullVector().z);
	}
	else {
		// Collision does not match any of the faces
	}
}

