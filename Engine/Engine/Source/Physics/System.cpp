#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Physics/Surfaces.h"
#include "Physics/Vector/Math.h"

std::vector<std::weak_ptr<Engine::Physics::MotionComponent>> Engine::Physics::objectMotionComponents;

const Engine::Vec3<Engine::Units::MetersPerSecondSquared> gravity(0.f, -9.8f, 0.f);

void Engine::Physics::applyForce(const std::shared_ptr<MotionComponent>& component, const Vec3<Force>& force) {
	if (isZero(force)) {
		return;
	}

	const auto acceleration = Engine::Vec3<Units::MetersPerSecondSquared>(
		force.as<Units::Newtons>() /
		std::max(component->mass.as<Units::Kilograms>(), 0.0001f)
	);

	component->acceleration += acceleration;
}

void Engine::Physics::applyImpulse(const std::shared_ptr<MotionComponent>& component, const Vec3<Force>& force, const Time& duration) {
	if (isZero(force)) {
		return;
	}

	const auto deltaVelocity = Engine::Vec3<Units::MetersPerSecond>(
		force.as<Units::Newtons>() * duration.as<Units::Seconds>() / std::max(component->mass.as<Units::Kilograms>(), 0.0001f)
	);

	component->velocity += deltaVelocity;
}

void updateGravity(const std::shared_ptr<Engine::Physics::MotionComponent>& component) {
	if (!component->affectedByGravity) {
		return;
	}

	if (component->airborne) {
		const auto gravityForce = Engine::Vec3<Engine::Units::Newtons>(
			gravity.as<Engine::Units::MetersPerSecondSquared>() *
			component->mass.as<Engine::Units::Kilograms>()
		);
		applyForce(component, gravityForce);
	}
	else {
		component->acceleration.rawVec3().y = std::max(0.f, component->acceleration.rawVec3().y);
	}
}

void updateAirResistance(const std::shared_ptr<Engine::Physics::MotionComponent>& component) {
	constexpr float airDensity = 1.225f;												  // kg/m^3 (air density at sea level)
	const Engine::Units::Unitless dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;											  // Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const Engine::Force dragForceMagnitude(
		Engine::Units::Newtons(
			0.5f * dragCoefficient * airDensity * crossSectionalArea * 
			magnitude(component->velocity).as<Engine::Units::MetersPerSecond>() *
			magnitude(component->velocity).as<Engine::Units::MetersPerSecond>()
		)
	);

	applyForce(component, Engine::Vec3<Engine::Units::Newtons>(-dragForceMagnitude.as<Engine::Units::Newtons>() * normalize(component->velocity).rawVec3()));
}

void updateFriction(const std::shared_ptr<Engine::Physics::MotionComponent>& component, const Engine::Surfaces::SurfaceProperties& surface) {
	if (component->airborne) {
		return;
	}

	const Engine::Units::Newtons normal = component->mass.as<Engine::Units::Kilograms>() * magnitude(gravity).as<Engine::Units::MetersPerSecondSquared>();
	Engine::Units::Newtons friction = normal * surface.frictionCoefficient;

	if (component->selfControlled) {
		friction *= 5.f;
	}

	const Engine::Vec3<Engine::Units::Newtons> frictionForce(-friction * normalize(component->velocity).rawVec3());

	applyForce(component, frictionForce);
}

void updateVelocity(const std::shared_ptr<Engine::Physics::MotionComponent>& component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();

	if (component->selfControlled && !component->airborne) {
		const Engine::Units::Unitless dampingFactor = 5.0f; 
		component->velocity *= std::max(0.f, 1.0f - dampingFactor * deltaTime);
	}

	// Update velocity using the kinematic equation: v = v0 + at
	component->velocity += Engine::Vec3<Engine::Units::MetersPerSecond>(component->acceleration.as<Engine::Units::MetersPerSecondSquared>() * deltaTime);

	if (magnitude(component->velocity) > component->maxSpeed && !component->airborne) {
		component->velocity = Engine::Vec3<Engine::Units::MetersPerSecond>(
			normalize(component->velocity) * component->maxSpeed.as<Engine::Units::MetersPerSecond>()
		);
	}

	component->acceleration = { 0.f, 0.f, 0.f };
}

void updatePosition(const std::shared_ptr<Engine::Physics::MotionComponent>& component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Units::Seconds>();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->position += Engine::Vec3<Engine::Units::Meters>(
		component->velocity.as<Engine::Units::MetersPerSecond>() * deltaTime + 0.5f * 
		component->acceleration.as<Engine::Units::MetersPerSecondSquared>() * deltaTime * deltaTime
	);
}

void Engine::Physics::updateEntity(const std::shared_ptr<MotionComponent>& component) {
	if (isZero(component->velocity) && isZero(component->acceleration)) {
		component->moving = false;
	}
	else {
		component->moving = true;
	}

	updateGravity(component);
	updateAirResistance(component);
	updateVelocity(component);
	updatePosition(component);
}

void Engine::Physics::updateEntities() {
	std::erase_if(objectMotionComponents, [](const std::weak_ptr<MotionComponent>& obj) {
		return obj.expired();
	});

	for (auto& motionComponent : objectMotionComponents) {
		if (const auto motionComponentPtr = motionComponent.lock()) {
			updateEntity(motionComponentPtr);
		}
	}
}

void resolveAxisCollision(const int axis, Engine::BoundingBox& dynamicBoundingBox, const std::weak_ptr<Engine::Physics::MotionComponent>& dynamicMotionComponent, const Engine::Surfaces::SurfaceProperties& surface) {
	if (const auto dynamicMotionComponentPtr = dynamicMotionComponent.lock()) {
		dynamicMotionComponentPtr->velocity.rawVec3()[axis] = std::max(0.f, dynamicMotionComponentPtr->velocity.rawVec3()[axis]);
		dynamicMotionComponentPtr->acceleration.rawVec3()[axis] = std::max(0.f, dynamicMotionComponentPtr->acceleration.rawVec3()[axis]);

		if (axis == 1) {
			dynamicMotionComponent.airborne = false;
			updateFriction(dynamicMotionComponentPtr, surface);
		}

		dynamicMotionComponentPtr->position.rawVec3()[axis] = dynamicBoundingBox.lowerBound.rawVec3()[axis] - (dynamicBoundingBox.lowerBound.rawVec3()[axis] - dynamicMotionComponentPtr->position.rawVec3()[axis]);
	}
}

void Engine::Physics::resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	const std::vector<std::pair<Vec3<Length>, int>> bounds = {
		{getLeftBound(dynamicBoundingBox), 0},
		{getRightBound(dynamicBoundingBox), 0},
		{getFrontBound(dynamicBoundingBox), 2},
		{getBackBound(dynamicBoundingBox), 2},
		{dynamicBoundingBox.lowerBound, 1},
		{dynamicBoundingBox.upperBound, 0}
	};

	for (const auto& [bound, axis] : bounds) {
		if (epsilonEqualIndex(collisionInfo.collisionPoint, bound, axis)) {
			resolveAxisCollision(axis, dynamicBoundingBox, dynamicMotionComponent, getSurfaceProperties(Surfaces::SurfaceType::Concrete));
			return;
		}
	}
}

