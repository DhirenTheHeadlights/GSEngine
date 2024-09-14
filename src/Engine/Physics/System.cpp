#include "Engine/Physics/System.h"

using namespace Engine;

std::vector<Physics::MotionComponent*> Physics::components;

void Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Physics::MotionComponent* component, const float deltaTime) {
	component->velocity.y -= 9.8f * deltaTime;
}

void updateAirResistance(Physics::MotionComponent* component, const float deltaTime) {
	// Should be more complex, but for now, just a simple drag force
	constexpr float dragCoefficient = 0.47f;	// Sphere
	constexpr float airDensity = 1.225f;		// At 15 degrees Celsius
	constexpr float crossSectionalArea = 1.0f;	// Sphere
	const float dragForce = 0.5f * dragCoefficient * airDensity * crossSectionalArea * glm::length(component->velocity);
	const glm::vec3 dragDirection = glm::normalize(component->velocity);
	const glm::vec3 dragAcceleration = dragDirection * dragForce / component->mass;
	component->acceleration -= dragAcceleration * deltaTime;
}

void updateVelocity(Physics::MotionComponent* component, const float deltaTime) {
	component->velocity += component->acceleration * deltaTime;
}

void Physics::updateEntities(const float deltaTime) {
	for (MotionComponent* component : components) {
		updateGravity(component, deltaTime);
		if (component->airborne) updateAirResistance(component, deltaTime);
		updateVelocity(component, deltaTime);
	}
}