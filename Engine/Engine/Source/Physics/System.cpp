#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Physics/Surfaces.h"
#include "Physics/Vector/Math.h"

namespace {
	auto g_gravity = gse::vec3<gse::units::meters_per_second_squared>(0.f, -9.8f, 0.f);
}

void gse::physics::apply_force(motion_component* component, const vec3<force>& force) {
	if (is_zero(force)) {
		return;
	}

	const auto acceleration = gse::vec3<units::meters_per_second_squared>(
		force.as<units::newtons>() /
		std::max(component->mass.as<units::kilograms>(), 0.0001f)
	);

	component->current_acceleration += acceleration;
}

void gse::physics::apply_impulse(motion_component* component, const vec3<force>& force, const time& duration) {
	if (is_zero(force)) {
		return;
	}

	const auto delta_velocity = gse::vec3<units::meters_per_second>(
		force.as<units::newtons>() * duration.as<units::seconds>() / std::max(component->mass.as<units::kilograms>(), 0.0001f)
	);

	component->current_velocity += delta_velocity;
}

void gse::physics::group::add_motion_component(const std::shared_ptr<motion_component>& object) {
	m_motion_components.push_back(object);
}

void gse::physics::group::remove_motion_component(const std::shared_ptr<motion_component>& object) {
	std::erase_if(m_motion_components, [&](const std::weak_ptr<motion_component>& obj) {
		return !obj.owner_before(object) && !object.owner_before(obj);
		});
}

namespace {
	void update_gravity(gse::physics::motion_component* component) {
		if (!component->affected_by_gravity) {
			return;
		}

		if (component->airborne) {
			const auto gravity_force = gse::vec3<gse::units::newtons>(
				g_gravity.as<gse::units::meters_per_second_squared>() *
				component->mass.as<gse::units::kilograms>()
			);
			apply_force(component, gravity_force);
		}
		else {
			component->current_acceleration.as_default_units().y = std::max(0.f, component->current_acceleration.as_default_units().y);
		}
	}

	void update_air_resistance(gse::physics::motion_component* component) {
		constexpr float air_density = 1.225f;												  // kg/m^3 (air density at sea level)
		const gse::unitless drag_coefficient = component->airborne ? 0.47f : 1.05f;			  // Approx for a sphere vs a box
		constexpr float cross_sectional_area = 1.0f;										  // Example area in m^2, adjust according to the object

		// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
		const gse::force drag_force_magnitude = gse::newtons(
			0.5f * drag_coefficient.as_default_unit() * air_density * cross_sectional_area *
			magnitude(component->current_velocity).as<gse::units::meters_per_second>() *
			magnitude(component->current_velocity).as<gse::units::meters_per_second>()
		);

		apply_force(component, gse::vec3<gse::units::newtons>(-drag_force_magnitude.as<gse::units::newtons>() * normalize(component->current_velocity).as_default_units()));
	}

	void update_friction(gse::physics::motion_component* component, const gse::surfaces::surface_properties& surface) {
		if (component->airborne) {
			return;
		}

		const gse::force normal = gse::newtons(component->mass.as<gse::units::kilograms>() * magnitude(g_gravity).as<gse::units::meters_per_second_squared>());
		gse::force friction = normal * surface.friction_coefficient;

		if (component->self_controlled) {
			friction *= 5.f;
		}

		const gse::vec3<gse::units::newtons> friction_force(-friction.as<gse::units::newtons>() * normalize(component->current_velocity).as<gse::units::meters_per_second>());

		apply_force(component, friction_force);
	}

	void update_velocity(gse::physics::motion_component* component) {
		const float delta_time = gse::main_clock::get_delta_time().as<gse::units::seconds>();

		if (component->self_controlled && !component->airborne) {
			const gse::unitless damping_factor = 5.0f;
			component->current_velocity *= std::max(0.f, 1.0f - damping_factor.as_default_unit() * delta_time);
		}

		// Update current_velocity using the kinematic equation: v = v0 + at
		component->current_velocity += gse::vec3<gse::units::meters_per_second>(component->current_acceleration.as<gse::units::meters_per_second_squared>() * delta_time);

		if (magnitude(component->current_velocity) > component->max_speed && !component->airborne) {
			component->current_velocity = gse::vec3<gse::units::meters_per_second>(
				normalize(component->current_velocity) * component->max_speed.as<gse::units::meters_per_second>()
			);
		}

		component->current_acceleration = { 0.f, 0.f, 0.f };
	}

	void update_position(gse::physics::motion_component* component) {
		const float delta_time = gse::main_clock::get_delta_time().as<gse::units::seconds>();

		// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
		component->current_position += gse::vec3<gse::units::meters>(
			component->current_velocity.as<gse::units::meters_per_second>() * delta_time + 0.5f *
			component->current_acceleration.as<gse::units::meters_per_second_squared>() * delta_time * delta_time
		);
	}
}

void gse::physics::update_entity(motion_component* component) {
	if (is_zero(component->current_velocity) && is_zero(component->current_acceleration)) {
		component->moving = false;
	}
	else {
		component->moving = true;
	}

	update_gravity(component);
	update_air_resistance(component);
	update_velocity(component);
	update_position(component);
}

void gse::physics::group::update() {
	std::erase_if(m_motion_components, [](const std::weak_ptr<motion_component>& obj) {
		return obj.expired();
	});

	for (auto& motion_component : m_motion_components) {
		if (const auto motion_component_ptr = motion_component.lock()) {
			update_entity(motion_component_ptr.get());
		}
	}
}

void gse::physics::resolve_collision(bounding_box& dynamic_bounding_box, motion_component* dynamic_motion_component, const collision_information& collision_info) {
	float& vel = dynamic_motion_component->current_velocity.as_default_units()[collision_info.get_axis()];
	float& acc = dynamic_motion_component->current_acceleration.as_default_units()[collision_info.get_axis()];

	// Project current_velocity and acceleration onto collision normal to check movement toward the surface
	const float velocity_into_surface = dot(dynamic_motion_component->current_velocity, collision_info.collision_normal);
	const float acceleration_into_surface = dot(dynamic_motion_component->current_acceleration, collision_info.collision_normal);

	// Set current_velocity and acceleration to zero along the collision normal if moving into the surface
	if (velocity_into_surface < 0) {
		vel = 0;
	}
	if (acceleration_into_surface < 0) {
		acc = 0;
	}

	const vec3<units::meters> correction = collision_info.collision_normal * collision_info.penetration.as<units::meters>();
	dynamic_motion_component->current_position += correction;

	// Special case for ground collision (assumed Y-axis collision)
	if (collision_info.get_axis() == 1 && collision_info.collision_normal.as_default_units().y > 0) {
		dynamic_motion_component->airborne = false;
		update_friction(dynamic_motion_component, get_surface_properties(surfaces::surface_type::concrete));
	}
}

