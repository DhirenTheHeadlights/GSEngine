#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Physics/Surfaces.h"
#include "Physics/Vector/Math.h"

auto gravity = gse::vec3<gse::meters_per_second_squared>(0.f, -9.8f, 0.f);

void gse::physics::apply_force(motion_component* component, const vec3<force>& force) {
	if (isZero(force)) {
		return;
	}

	const auto acceleration = gse::vec3<meters_per_second_squared>(
		force.as<newtons>() /
		std::max(component->mass.as<kilograms>(), 0.0001f)
	);

	component->current_acceleration += acceleration;
}

void gse::physics::apply_impulse(motion_component* component, const vec3<force>& force, const time& duration) {
	if (isZero(force)) {
		return;
	}

	const auto deltaVelocity = gse::vec3<meters_per_second>(
		force.as<newtons>() * duration.as<Seconds>() / std::max(component->mass.as<kilograms>(), 0.0001f)
	);

	component->current_velocity += deltaVelocity;
}

void gse::physics::group::add_motion_component(const std::shared_ptr<motion_component>& object) {
	m_motion_components.push_back(object);
}

void gse::physics::group::remove_motion_component(const std::shared_ptr<motion_component>& object) {
	std::erase_if(m_motion_components, [&](const std::weak_ptr<motion_component>& obj) {
		return !obj.owner_before(object) && !object.owner_before(obj);
		});
}

void updateGravity(gse::physics::motion_component* component) {
	if (!component->affected_by_gravity) {
		return;
	}

	if (component->airborne) {
		const auto gravityForce = gse::vec3<gse::newtons>(
			gravity.as<gse::meters_per_second_squared>() *
			component->mass.as<gse::kilograms>()
		);
		applyForce(component, gravityForce);
	}
	else {
		component->current_acceleration.as_default_units().y = std::max(0.f, component->current_acceleration.as_default_units().y);
	}
}

void updateAirResistance(gse::physics::motion_component* component) {
	constexpr float airDensity = 1.225f;												  // kg/m^3 (air density at sea level)
	const gse::unitless dragCoefficient = component->airborne ? 0.47f : 1.05f;  // Approx for a sphere vs a box
	constexpr float crossSectionalArea = 1.0f;											  // Example area in m^2, adjust according to the object

	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	const gse::force dragForceMagnitude = gse::newtons(
			0.5f * dragCoefficient.asDefaultUnit() * airDensity * crossSectionalArea * 
			magnitude(component->current_velocity).as<gse::meters_per_second>() *
			magnitude(component->current_velocity).as<gse::meters_per_second>()
	);

	applyForce(component, gse::vec3<gse::newtons>(-dragForceMagnitude.as<gse::newtons>() * normalize(component->current_velocity).as_default_units()));
}

void updateFriction(gse::physics::motion_component* component, const gse::surfaces::surface_properties& surface) {
	if (component->airborne) {
		return;
	}

	const gse::force normal = gse::newtons(component->mass.as<gse::kilograms>() * magnitude(gravity).as<gse::meters_per_second_squared>());
	gse::force friction = normal * surface.m_friction_coefficient;

	if (component->self_controlled) {
		friction *= 5.f;
	}

	const gse::vec3<gse::newtons> frictionForce(-friction.as<gse::newtons>() * normalize(component->current_velocity).as<gse::meters_per_second>());

	applyForce(component, frictionForce);
}

void updateVelocity(gse::physics::motion_component* component) {
	const float deltaTime = gse::main_clock::get_delta_time().as<gse::Seconds>();

	if (component->self_controlled && !component->airborne) {
		const gse::unitless dampingFactor = 5.0f; 
		component->current_velocity *= std::max(0.f, 1.0f - dampingFactor.asDefaultUnit() * deltaTime);
	}

	// Update current_velocity using the kinematic equation: v = v0 + at
	component->current_velocity += gse::vec3<gse::meters_per_second>(component->current_acceleration.as<gse::meters_per_second_squared>() * deltaTime);

	if (magnitude(component->current_velocity) > component->max_speed && !component->airborne) {
		component->current_velocity = gse::vec3<gse::meters_per_second>(
			normalize(component->current_velocity) * component->max_speed.as<gse::meters_per_second>()
		);
	}

	component->current_acceleration = { 0.f, 0.f, 0.f };
}

void updatePosition(gse::physics::motion_component* component) {
	const float deltaTime = gse::main_clock::get_delta_time().as<gse::Seconds>();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component->current_position += gse::vec3<gse::Meters>(
		component->current_velocity.as<gse::meters_per_second>() * deltaTime + 0.5f * 
		component->current_acceleration.as<gse::meters_per_second_squared>() * deltaTime * deltaTime
	);
}

void gse::physics::update_entity(motion_component* component) {
	if (isZero(component->current_velocity) && isZero(component->current_acceleration)) {
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

void gse::physics::group::update() {
	std::erase_if(m_motion_components, [](const std::weak_ptr<motion_component>& obj) {
		return obj.expired();
	});

	for (auto& motionComponent : m_motion_components) {
		if (const auto motionComponentPtr = motionComponent.lock()) {
			update_entity(motionComponentPtr.get());
		}
	}
}

void gse::physics::resolve_collision(bounding_box& dynamic_bounding_box, const std::weak_ptr<motion_component>& dynamic_motion_component, const collision_information& collision_info) {
	if (const auto dynamicMotionComponentPtr = dynamic_motion_component.lock()) {
		float& vel = dynamicMotionComponentPtr->current_velocity.as_default_units()[collision_info.get_axis()];
		float& acc = dynamicMotionComponentPtr->current_acceleration.as_default_units()[collision_info.get_axis()];

		// Project current_velocity and acceleration onto collision normal to check movement toward the surface
		const float velocityIntoSurface = dot(dynamicMotionComponentPtr->current_velocity, collision_info.collision_normal);
		const float accelerationIntoSurface = dot(dynamicMotionComponentPtr->current_acceleration, collision_info.collision_normal);

		// Set current_velocity and acceleration to zero along the collision normal if moving into the surface
		if (velocityIntoSurface < 0) {
			vel = 0;
		}
		if (accelerationIntoSurface < 0) {
			acc = 0;
		}

		// Special case for ground collision (assumed Y-axis collision)
		if (collision_info.get_axis() == 1 && collision_info.collision_normal.as_default_units().y > 0) {
			dynamicMotionComponentPtr->airborne = false;
			updateFriction(dynamicMotionComponentPtr.get(), get_surface_properties(surfaces::surface_type::concrete));
		}
	}
}

