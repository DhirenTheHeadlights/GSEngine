#include "Engine/Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Engine/Core/Clock.h"
#include "Engine/Physics/Surfaces.h"
#include "Engine/Physics/Vector/Math.h"

std::vector<Engine::Physics::MotionComponent*> Engine::Physics::components;

const Engine::Vec3<Engine::Units::MetersPerSecondSquared> gravity(0.f, -9.8f, 0.f);

void Engine::Physics::applyForce(MotionComponent* component, const Vec3<Force>& force) {
	auto acceleration = Engine::Vec3<Units::MetersPerSecondSquared>(
		force.as<Units::Newtons>() /
		std::max(component->mass.as<Units::Kilograms>(), 0.0001f)
	);

	if (component->airborne) {
		acceleration.rawVec3().x *= 0.1f;
		acceleration.rawVec3().z *= 0.1f;
	}

	component->acceleration += acceleration;
}

void Engine::Physics::addMotionComponent(MotionComponent& component) {
	components.push_back(&component);
}

void Engine::Physics::removeMotionComponent(MotionComponent& component) {
	std::erase(components, &component);
}

void updateGravity(Engine::Physics::MotionComponent* component) {
	if (component->affectedByGravity && component->airborne) {
		const auto gravityForce = Engine::Vec3<Engine::Units::Newtons>(gravity.as<Engine::Units::MetersPerSecondSquared>() * component->mass.as<Engine::Units::Kilograms>());

		applyForce(component, gravityForce);
	}
	else if (component->affectedByGravity && !component->airborne) {
		component->acceleration.rawVec3().y = std::max(0.f, component->acceleration.rawVec3().y);
	}
}

void updateAirResistance(Engine::Physics::MotionComponent* component) {
	// Constants
	constexpr float airDensity = 1.225f;								// kg/m^3 (air density at sea level)
	const float dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;							// Example area in m^2, adjust according to the object

	if (component->velocity.magnitude().as<Engine::Units::MetersPerSecond>() == 0.f) {
		return;
	}

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const Engine::Force dragForceMagnitude(
		Engine::Units::Newtons(
			0.5f * dragCoefficient * airDensity * crossSectionalArea * 
			component->velocity.magnitude().as<Engine::Units::MetersPerSecond>() *
			component->velocity.magnitude().as<Engine::Units::MetersPerSecond>()
		)
	); 

	// Apply the drag force to the object
	applyForce(component, Engine::Vec3<Engine::Units::Newtons>(-dragForceMagnitude.as<Engine::Units::Newtons>() * component->velocity.getDirection()));
}

void updateFriction(Engine::Physics::MotionComponent* component, const Engine::Surfaces::SurfaceProperties& surface) {
	const Engine::Units::Newtons normal = component->mass.as<Engine::Units::Kilograms>() * gravity.magnitude().as<Engine::Units::MetersPerSecondSquared>();
	const Engine::Units::Newtons friction = surface.frictionCoefficient * normal;

	const Engine::Vec3<Engine::Units::Newtons> frictionForce(-friction * component->velocity.getDirection());

	applyForce(component, frictionForce);
}

void updateVelocity(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();

	// Update velocity using the kinematic equation: v = v0 + at
	component->velocity += Engine::Vec3<Engine::Units::MetersPerSecond>(component->acceleration.as<Engine::Units::MetersPerSecondSquared>() * deltaTime);

	component->acceleration = Engine::Vec3<Engine::Units::MetersPerSecondSquared>(0.f, 0.f, 0.f);
}

void updatePosition(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->position += Engine::Vec3<Engine::Units::Meters>(
		component->velocity.as<Engine::Units::MetersPerSecond>() * deltaTime + 0.5f * 
		component->acceleration.as<Engine::Units::MetersPerSecondSquared>() * deltaTime * deltaTime
	);
}

void Engine::Physics::updateEntity(MotionComponent* component) {
	updateGravity(component);
	updateAirResistance(component);
	updateVelocity(component);
	updatePosition(component);
}

void Engine::Physics::updateEntities() {
	for (MotionComponent* component : components) {
		updateEntity(component);
	}
}

void resolveAxisCollision(int axis, Engine::BoundingBox& dynamicBoundingBox, Engine::Physics::MotionComponent& dynamicMotionComponent, const Engine::Surfaces::SurfaceProperties& surface) {
	dynamicMotionComponent.velocity.rawVec3()[axis] = std::max(0.f, dynamicMotionComponent.velocity.rawVec3()[axis]);
	dynamicMotionComponent.acceleration.rawVec3()[axis] = std::max(0.f, dynamicMotionComponent.acceleration.rawVec3()[axis]);

	if (axis == 1) {
		dynamicMotionComponent.airborne = false;
		updateFriction(&dynamicMotionComponent, surface);
	}

	dynamicMotionComponent.position.rawVec3()[axis] = dynamicBoundingBox.lowerBound.rawVec3()[axis] - (dynamicBoundingBox.lowerBound.rawVec3()[axis] - dynamicMotionComponent.position.rawVec3()[axis]);
}

void Engine::Physics::resolveCollision(BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	if (epsilonEqual(collisionInfo.collisionPoint, dynamicBoundingBox.lowerBound)) {
		resolveAxisCollision(0, dynamicBoundingBox, dynamicMotionComponent, Surfaces::getSurfaceProperties(Surfaces::SurfaceType::Concrete));
	}
}

