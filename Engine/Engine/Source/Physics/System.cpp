#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Physics/Surfaces.h"
#include "Physics/Vector/Math.h"

auto gravity = gse::Vec3<gse::MetersPerSecondSquared>(0.f, -9.8f, 0.f);

void gse::Physics::applyForce(MotionComponent* component, const Vec3<Force>& force) {
	if (isZero(force)) {
		return;
	}

	const auto acceleration = gse::Vec3<MetersPerSecondSquared>(
		force.as<Newtons>() /
		std::max(component->mass.as<Kilograms>(), 0.0001f)
	);

	component->acceleration += acceleration;
}

void gse::Physics::applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration) {
	if (isZero(force)) {
		return;
	}

	const auto deltaVelocity = gse::Vec3<MetersPerSecond>(
		force.as<Newtons>() * duration.as<Seconds>() / std::max(component->mass.as<Kilograms>(), 0.0001f)
	);

	component->velocity += deltaVelocity;
}

void gse::Physics::Group::addMotionComponent(const std::shared_ptr<MotionComponent>& object) {
	motionComponents.push_back(object);
}

void gse::Physics::Group::removeMotionComponent(const std::shared_ptr<MotionComponent>& object) {
	std::erase_if(motionComponents, [&](const std::weak_ptr<MotionComponent>& obj) {
		return !obj.owner_before(object) && !object.owner_before(obj);
		});
}

void updateGravity(gse::Physics::MotionComponent* component) {
	if (!component->affectedByGravity) {
		return;
	}

	if (component->airborne) {
		const auto gravityForce = gse::Vec3<gse::Newtons>(
			gravity.as<gse::MetersPerSecondSquared>() *
			component->mass.as<gse::Kilograms>()
		);
		applyForce(component, gravityForce);
	}
	else {
		component->acceleration.asDefaultUnits().y = std::max(0.f, component->acceleration.asDefaultUnits().y);
	}
}

void updateAirResistance(gse::Physics::MotionComponent* component) {
	constexpr float airDensity = 1.225f;												  // kg/m^3 (air density at sea level)
	const gse::Unitless dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;											  // Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const gse::Force dragForceMagnitude = gse::newtons(
			0.5f * dragCoefficient.asDefaultUnit() * airDensity * crossSectionalArea * 
			magnitude(component->velocity).as<gse::MetersPerSecond>() *
			magnitude(component->velocity).as<gse::MetersPerSecond>()
	);

	applyForce(component, gse::Vec3<gse::Newtons>(-dragForceMagnitude.as<gse::Newtons>() * normalize(component->velocity).asDefaultUnits()));
}

void updateFriction(gse::Physics::MotionComponent* component, const gse::Surfaces::SurfaceProperties& surface) {
	if (component->airborne) {
		return;
	}

	const gse::Force normal = gse::newtons(component->mass.as<gse::Kilograms>() * magnitude(gravity).as<gse::MetersPerSecondSquared>());
	gse::Force friction = normal * surface.frictionCoefficient;

	if (component->selfControlled) {
		friction *= 5.f;
	}

	const gse::Vec3<gse::Newtons> frictionForce(-friction.as<gse::Newtons>() * normalize(component->velocity).as<gse::MetersPerSecond>());

	applyForce(component, frictionForce);
}

void updateVelocity(gse::Physics::MotionComponent* component) {
	const float deltaTime = gse::MainClock::getDeltaTime().as<gse::Seconds>();

	if (component->selfControlled && !component->airborne) {
		const gse::Unitless dampingFactor = 5.0f; 
		component->velocity *= std::max(0.f, 1.0f - dampingFactor.asDefaultUnit() * deltaTime);
	}

	// Update velocity using the kinematic equation: v = v0 + at
	component->velocity += gse::Vec3<gse::MetersPerSecond>(component->acceleration.as<gse::MetersPerSecondSquared>() * deltaTime);

	if (magnitude(component->velocity) > component->maxSpeed && !component->airborne) {
		component->velocity = gse::Vec3<gse::MetersPerSecond>(
			normalize(component->velocity) * component->maxSpeed.as<gse::MetersPerSecond>()
		);
	}

	component->acceleration = { 0.f, 0.f, 0.f };
}

void updatePosition(gse::Physics::MotionComponent* component) {
	const float deltaTime = gse::MainClock::getDeltaTime().as<gse::Seconds>();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->position += gse::Vec3<gse::Meters>(
		component->velocity.as<gse::MetersPerSecond>() * deltaTime + 0.5f * 
		component->acceleration.as<gse::MetersPerSecondSquared>() * deltaTime * deltaTime
	);
}

void gse::Physics::updateEntity(MotionComponent* component) {
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

void gse::Physics::Group::update() {
	std::erase_if(motionComponents, [](const std::weak_ptr<MotionComponent>& obj) {
		return obj.expired();
	});

	for (auto& motionComponent : motionComponents) {
		if (const auto motionComponentPtr = motionComponent.lock()) {
			updateEntity(motionComponentPtr.get());
		}
	}
}

void gse::Physics::resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	if (const auto dynamicMotionComponentPtr = dynamicMotionComponent.lock()) {
		float& vel = dynamicMotionComponentPtr->velocity.asDefaultUnits()[collisionInfo.getAxis()];
		float& acc = dynamicMotionComponentPtr->acceleration.asDefaultUnits()[collisionInfo.getAxis()];

		// Project velocity and acceleration onto collision normal to check movement toward the surface
		const float velocityIntoSurface = dot(dynamicMotionComponentPtr->velocity, collisionInfo.collisionNormal);
		const float accelerationIntoSurface = dot(dynamicMotionComponentPtr->acceleration, collisionInfo.collisionNormal);

		// Set velocity and acceleration to zero along the collision normal if moving into the surface
		if (velocityIntoSurface < 0) {
			vel = 0;
		}
		if (accelerationIntoSurface < 0) {
			acc = 0;
		}

		// Special case for ground collision (assumed Y-axis collision)
		if (collisionInfo.getAxis() == 1 && collisionInfo.collisionNormal.asDefaultUnits().y > 0) {
			dynamicMotionComponentPtr->airborne = false;
			updateFriction(dynamicMotionComponentPtr.get(), getSurfaceProperties(Surfaces::SurfaceType::Concrete));
		}
	}
}

