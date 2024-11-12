#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Physics/Surfaces.h"
#include "Physics/Vector/Math.h"

auto gravity = Engine::Vec3<Engine::MetersPerSecondSquared>(0.f, -9.8f, 0.f);

void Engine::Physics::applyForce(MotionComponent* component, const Vec3<Force>& force) {
	if (isZero(force)) {
		return;
	}

	const auto acceleration = Engine::Vec3<MetersPerSecondSquared>(
		force.as<Newtons>() /
		std::max(component->mass.as<Kilograms>(), 0.0001f)
	);

	component->acceleration += acceleration;
}

void Engine::Physics::applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration) {
	if (isZero(force)) {
		return;
	}

	const auto deltaVelocity = Engine::Vec3<MetersPerSecond>(
		force.as<Newtons>() * duration.as<Seconds>() / std::max(component->mass.as<Kilograms>(), 0.0001f)
	);

	component->velocity += deltaVelocity;
}

void Engine::Physics::System::addMotionComponent(const std::shared_ptr<MotionComponent>& object) {
	objectMotionComponents.push_back(object);
}

void Engine::Physics::System::removeMotionComponent(const std::shared_ptr<MotionComponent>& object) {
	std::erase_if(objectMotionComponents, [&](const std::weak_ptr<MotionComponent>& obj) {
		return !obj.owner_before(object) && !object.owner_before(obj);
		});
}

void updateGravity(Engine::Physics::MotionComponent* component) {
	if (!component->affectedByGravity) {
		return;
	}

	if (component->airborne) {
		const auto gravityForce = Engine::Vec3<Engine::Newtons>(
			gravity.as<Engine::MetersPerSecondSquared>() *
			component->mass.as<Engine::Kilograms>()
		);
		applyForce(component, gravityForce);
	}
	else {
		component->acceleration.rawVec3().y = std::max(0.f, component->acceleration.rawVec3().y);
	}
}

void updateAirResistance(Engine::Physics::MotionComponent* component) {
	constexpr float airDensity = 1.225f;												  // kg/m^3 (air density at sea level)
	const Engine::Unitless dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;											  // Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const Engine::Force dragForceMagnitude(
		Engine::Newtons(
			0.5f * dragCoefficient.asDefaultUnit() * airDensity * crossSectionalArea * 
			magnitude(component->velocity).as<Engine::MetersPerSecond>() *
			magnitude(component->velocity).as<Engine::MetersPerSecond>()
		)
	);

	applyForce(component, Engine::Vec3<Engine::Newtons>(-dragForceMagnitude.as<Engine::Newtons>() * normalize(component->velocity).rawVec3()));
}

void updateFriction(Engine::Physics::MotionComponent* component, const Engine::Surfaces::SurfaceProperties& surface) {
	if (component->airborne) {
		return;
	}

	const Engine::Newtons normal = component->mass.as<Engine::Kilograms>() * magnitude(gravity).as<Engine::MetersPerSecondSquared>();
	Engine::Newtons friction = normal * surface.frictionCoefficient;

	if (component->selfControlled) {
		friction *= 5.f;
	}

	const Engine::Vec3<Engine::Newtons> frictionForce(-friction * normalize(component->velocity).rawVec3());

	applyForce(component, frictionForce);
}

void updateVelocity(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Seconds>();

	if (component->selfControlled && !component->airborne) {
		const Engine::Unitless dampingFactor = 5.0f; 
		component->velocity *= std::max(0.f, 1.0f - dampingFactor.asDefaultUnit() * deltaTime);
	}

	// Update velocity using the kinematic equation: v = v0 + at
	component->velocity += Engine::Vec3<Engine::MetersPerSecond>(component->acceleration.as<Engine::MetersPerSecondSquared>() * deltaTime);

	if (magnitude(component->velocity) > component->maxSpeed && !component->airborne) {
		component->velocity = Engine::Vec3<Engine::MetersPerSecond>(
			normalize(component->velocity) * component->maxSpeed.as<Engine::MetersPerSecond>()
		);
	}

	component->acceleration = { 0.f, 0.f, 0.f };
}

void updatePosition(Engine::Physics::MotionComponent* component) {
	const float deltaTime = Engine::MainClock::getDeltaTime().as<Engine::Seconds>();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->position += Engine::Vec3<Engine::Meters>(
		component->velocity.as<Engine::MetersPerSecond>() * deltaTime + 0.5f * 
		component->acceleration.as<Engine::MetersPerSecondSquared>() * deltaTime * deltaTime
	);
}

void Engine::Physics::System::updateEntity(MotionComponent* component) {
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

void Engine::Physics::System::update() {
	std::erase_if(objectMotionComponents, [](const std::weak_ptr<MotionComponent>& obj) {
		return obj.expired();
	});

	for (auto& motionComponent : objectMotionComponents) {
		if (const auto motionComponentPtr = motionComponent.lock()) {
			updateEntity(motionComponentPtr.get());
		}
	}
}

void resolveAxisCollision(
	const Engine::CollisionInformation& collisionInfo,
	const std::weak_ptr<Engine::Physics::MotionComponent>& dynamicMotionComponent,
	const Engine::Surfaces::SurfaceProperties& surface
) {
	if (const auto dynamicMotionComponentPtr = dynamicMotionComponent.lock()) {
		float& vel = dynamicMotionComponentPtr->velocity.rawVec3()[collisionInfo.getAxis()];
		float& acc = dynamicMotionComponentPtr->acceleration.rawVec3()[collisionInfo.getAxis()];

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
		if (collisionInfo.getAxis() == 1 && collisionInfo.collisionNormal.rawVec3().y > 0) {
			dynamicMotionComponentPtr->airborne = false;
			updateFriction(dynamicMotionComponentPtr.get(), surface);
		}

		// Adjust position to prevent sinking into the collision surface
		//dynamicMotionComponentPtr->position.rawVec3()[collisionInfo.getAxis()] -= collisionInfo.penetration.as<Engine::Meters>();
	}
}

void Engine::Physics::System::resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo) {
	// Determine which axis to resolve using collisionInfo
	resolveAxisCollision(collisionInfo, dynamicMotionComponent, getSurfaceProperties(Surfaces::SurfaceType::Concrete));
}

