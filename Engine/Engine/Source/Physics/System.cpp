#include "Physics/System.h"

#include <algorithm>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "Core/Clock.h"
#include "Core/ObjectRegistry.h"
#include "Graphics/RenderComponent.h"
#include "Physics/Surfaces.h"
#include "Physics/Collision/BroadPhaseCollisions.h"
#include "Physics/Collision/CollisionComponent.h"
#include "Physics/Vector/Math.h"
#include "Platform/GLFW/Input.h"

namespace {
	auto g_gravity = gse::vec3<gse::units::meters_per_second_squared>(0.f, -9.8f, 0.f);
}

auto gse::physics::apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto acceleration = gse::vec3<units::meters_per_second_squared>(
		force.as<units::newtons>() /
		std::max(component.mass.as<units::kilograms>(), 0.0001f)
	);

	vec3<length> center_of_mass;

	if (const auto* render_component = gse::registry::get_component_ptr<gse::render_component>(component.parent_id); render_component) {
		center_of_mass = render_component->center_of_mass;
	}
	else {
		center_of_mass = component.current_position;
	}

	component.current_torque += gse::vec3<units::newton_meters>(glm::cross((world_force_position - center_of_mass).as<units::meters>(), force.as<units::newtons>()));
	component.current_acceleration += acceleration;
}

auto gse::physics::apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto delta_velocity = gse::vec3<units::meters_per_second>(
		force.as<units::newtons>() * duration.as<units::seconds>() / std::max(component.mass.as<units::kilograms>(), 0.0001f)
	);

	component.current_velocity += delta_velocity;
}

namespace {
	auto update_friction(gse::physics::motion_component& component, const gse::surfaces::surface_properties& surface) -> void {
		if (component.airborne) {
			return;
		}

		const gse::force normal = gse::newtons(component.mass.as<gse::units::kilograms>() * magnitude(g_gravity).as<gse::units::meters_per_second_squared>());
		gse::force friction = normal * surface.friction_coefficient;

		if (component.self_controlled) {
			friction *= 5.f;
		}

		const gse::vec3<gse::units::newtons> friction_force(-friction.as<gse::units::newtons>() * normalize(component.current_velocity).as<gse::units::meters_per_second>());

		apply_force(component, friction_force, component.current_position);
	}

	auto update_gravity(gse::physics::motion_component& component) -> void {
		if (!component.affected_by_gravity) {
			return;
		}

		if (component.airborne) {
			const auto gravity_force = gse::vec3<gse::units::newtons>(
				g_gravity.as<gse::units::meters_per_second_squared>() *
				component.mass.as<gse::units::kilograms>()
			);
			apply_force(component, gravity_force, component.current_position);
		}
		else {
			component.current_acceleration.as_default_units().y = std::max(0.f, component.current_acceleration.as_default_units().y);
			update_friction(component, get_surface_properties(gse::surfaces::surface_type::concrete));
		}
	}

	auto update_air_resistance(gse::physics::motion_component& component) -> void {
		// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
		for (int i = 0; i < 3; ++i) {
			if (const float velocity = component.current_velocity.as_default_units()[i]; velocity != 0.0f) {
				constexpr float air_density = 1.225f;												  // kg/m^3 (air density at sea level)
				const gse::unitless drag_coefficient = component.airborne ? 0.47f : 1.05f;			  // Approx for a sphere vs a box
				constexpr float cross_sectional_area = 1.0f;										  // Example area in m^2, adjust according to the object

				const float drag_force_magnitude = 0.5f * drag_coefficient.as_default_unit() * air_density * cross_sectional_area * velocity * velocity;
				gse::unitless direction = velocity > 0 ? -1.0f : 1.0f;
				auto drag_force = gse::vec3<gse::units::newtons>(
					i == 0 ? drag_force_magnitude * direction : 0.0f,
					i == 1 ? drag_force_magnitude * direction : 0.0f,
					i == 2 ? drag_force_magnitude * direction : 0.0f
				);
				apply_force(component, drag_force, component.current_position);
			}
		}
	}

	auto update_velocity(gse::physics::motion_component& component) -> void {
		const float delta_time = gse::main_clock::get_constant_update_time().as<gse::units::seconds>();

		if (component.self_controlled && !component.airborne) {
			const gse::unitless damping_factor = 5.0f;
			component.current_velocity *= std::max(0.f, 1.0f - damping_factor.as_default_unit() * delta_time);
		}

		// Update current_velocity using the kinematic equation: v = v0 + at
		component.current_velocity += gse::vec3<gse::units::meters_per_second>(component.current_acceleration.as<gse::units::meters_per_second_squared>() * delta_time);

		if (magnitude(component.current_velocity) > component.max_speed && !component.airborne) {
			component.current_velocity = gse::vec3<gse::units::meters_per_second>(
				normalize(component.current_velocity) * component.max_speed.as<gse::units::meters_per_second>()
			);
		}

		if (fabs(component.current_velocity.as_default_units().x) < 0.0001f) component.current_velocity.as_default_units().x = 0.f;
		if (fabs(component.current_velocity.as_default_units().y) < 0.0001f) component.current_velocity.as_default_units().y = 0.f;
		if (fabs(component.current_velocity.as_default_units().z) < 0.0001f) component.current_velocity.as_default_units().z = 0.f;

		component.current_acceleration = { 0.f, 0.f, 0.f };
	}

	auto update_position(gse::physics::motion_component& component) -> void {
		const float delta_time = gse::main_clock::get_constant_update_time().as<gse::units::seconds>();

		// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
		component.current_position += gse::vec3<gse::units::meters>(
			component.current_velocity.as<gse::units::meters_per_second>() * delta_time + 0.5f *
			component.current_acceleration.as<gse::units::meters_per_second_squared>() * delta_time * delta_time
		);
	}

	auto update_rotation(gse::physics::motion_component& component) -> void {
		const float dt = gse::main_clock::get_constant_update_time().as<gse::units::seconds>();

		const auto alpha = gse::vec3<gse::units::radians_per_second_squared>(
			component.current_torque.as<gse::units::newton_meters>() / component.moment_of_inertia.as_default_unit() /* kg-m^2 */
		);

		component.angular_velocity += gse::vec3<gse::units::radians_per_second>(
			alpha.as<gse::units::radians_per_second_squared>() * dt
		);

		component.current_torque = { 0.f, 0.f, 0.f };

		const glm::vec3 angular_velocity = component.angular_velocity.as_default_units();
		const glm::quat omega_quaternion = { 0.f, angular_velocity.x, angular_velocity.y, angular_velocity.z };

		// dQ = 0.5 * omega_quaternion * orientation
		const glm::quat delta_quaternion = 0.5f * omega_quaternion * component.orientation;
		component.orientation += delta_quaternion * dt;
		component.orientation = normalize(component.orientation);
	}

	auto update_obb(const gse::physics::motion_component& motion_component) {
		if (auto* collision_component = gse::registry::get_component_ptr<gse::physics::collision_component>(motion_component.parent_id); collision_component) {
			auto& obb = collision_component->oriented_bounding_box;
			obb.center = motion_component.current_position;
			obb.orientation = motion_component.orientation;
			obb.update_axes();
		}
	}
}

auto gse::physics::update_object(motion_component& component) -> void {
	if (is_zero(component.current_velocity) && is_zero(component.current_acceleration)) {
		component.moving = false;
	}
	else {
		component.moving = true;
	}
	
	update_gravity(component);
	update_air_resistance(component);
	update_velocity(component);
	update_position(component);
	update_rotation(component);
	update_obb(component);
}

namespace {
	const gse::time g_max_time_step = gse::seconds(0.25);
	gse::time g_accumulator;
}

auto gse::physics::update() -> void {
	const time fixed_update_time = main_clock::get_constant_update_time();
	time frame_time = main_clock::get_raw_delta_time();

	if (frame_time > g_max_time_step) {
		frame_time = g_max_time_step;
	}

	g_accumulator += frame_time;

	while (g_accumulator >= fixed_update_time) {
		for (int i = 0; i < 5; i++) {
			broad_phase_collision::update();
		}

		for (auto& object : gse::registry::get_components<motion_component>()) {
			update_object(object);
		}

		g_accumulator -= fixed_update_time;
	}
}

