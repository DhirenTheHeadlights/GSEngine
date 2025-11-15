export module gse.physics:system;

import std;

import :surfaces;
import :collision_component;
import :motion_component;

import gse.platform;
import gse.physics.math;
import gse.utility;

export namespace gse::physics {
	auto apply_force(
		motion_component& component,
		const vec3<force>& force,
		const vec3<length>& world_force_position = { 0.f, 0.f, 0.f }
	) -> void;

	auto apply_impulse(
		motion_component& component,
		const vec3<force>& force,
		const time& duration
	) -> void;

	auto update_object(
		motion_component& component,
		collision_component* collision_component
	) -> void;
}

namespace gse::physics {
	constexpr auto gravity = gse::vec3<acceleration>(0.f, -9.8f, 0.f);

	using time_step = time_t<float, seconds>;

	auto update_friction(
		motion_component& component,
		const surfaces::surface_properties& surface
	) -> void;

	auto update_gravity(
		motion_component& component
	) -> void;

	auto update_air_resistance(
		motion_component& component
	) -> void;

	auto update_velocity(
		motion_component& component
	) -> void;

	auto update_position(
		motion_component& component
	) -> void;

	auto update_rotation(
		motion_component& component
	) -> void;

	auto update_bb(
		const motion_component& motion_component,
		collision_component& collision_component
	) -> void;
}

auto gse::physics::apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto acceleration = min(force / std::max(component.mass, kilograms(0.0001f)), vec3<gse::acceleration>(meters_per_second_squared(100.f)));

	const auto com = component.center_of_mass + component.current_position;

	component.accumulators.current_torque += cross(world_force_position - com, force);
	component.accumulators.current_acceleration += acceleration;
}

auto gse::physics::apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto delta_velocity = force * duration / std::max(component.mass, kilograms(0.0001f));

	component.current_velocity += delta_velocity;
}

auto gse::physics::update_friction(motion_component& component, const surfaces::surface_properties& surface) -> void {
	if (component.airborne) {
		return;
	}

	const force normal = component.mass * magnitude(gravity);
	force friction = normal * surface.friction_coefficient;

	if (component.self_controlled) {
		friction *= 5.f;
	}

	const vec3 friction_force(-friction * normalize(component.current_velocity));

	apply_force(component, friction_force, component.current_position);
}

auto gse::physics::update_gravity(motion_component& component) -> void {
	if (!component.affected_by_gravity) {
		return;
	}

	if (component.airborne) {
		const auto gravity_force = gravity * component.mass;
		apply_force(component, gravity_force, component.current_position);
	}
	else {
		component.accumulators.current_acceleration.y() = std::max(meters_per_second_squared(0.f), component.accumulators.current_acceleration.y());
		update_friction(component, get_surface_properties(surfaces::surface_type::concrete));
	}
}

auto gse::physics::update_air_resistance(motion_component& component) -> void {
	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	for (int i = 0; i < 3; ++i) {
		if (const velocity velocity = component.current_velocity[i]; velocity != meters_per_second(0.0f)) {
			constexpr density air_density = kilograms_per_cubic_meter(1.225f);
			const float drag_coefficient = component.airborne ? 0.47f : 1.05f;						
			constexpr area cross_sectional_area = square_meters(1.0f);											

			const force drag_force_magnitude = 0.5f * drag_coefficient * air_density * cross_sectional_area * velocity * velocity;
			const float direction = velocity > meters_per_second(0.f) ? -1.0f : 1.0f;
			auto drag_force = gse::vec3<force>(
				i == 0 ? drag_force_magnitude * direction : 0.0f,
				i == 1 ? drag_force_magnitude * direction : 0.0f,
				i == 2 ? drag_force_magnitude * direction : 0.0f
			);

			apply_force(component, drag_force, component.current_position);
		}
	}
}

auto gse::physics::update_velocity(motion_component& component) -> void {
	const auto delta_time = gse::system_clock::constant_update_time<time_step>();

	if (component.self_controlled && !component.airborne) {
		constexpr float damping_factor = 5.0f;
		component.current_velocity *= std::max(0.f, 1.f - damping_factor * delta_time.as<seconds>());
	}

	// Update current_velocity using the kinematic equation: v = v0 + at
	component.current_velocity += component.accumulators.current_acceleration * delta_time;

	if (magnitude(component.current_velocity) > component.max_speed && !component.airborne) {
		component.current_velocity = normalize(component.current_velocity) * component.max_speed;
	}

	if (is_zero(component.current_velocity)) {
		component.current_velocity = { 0.f, 0.f, 0.f };
	}
}

auto gse::physics::update_position(motion_component& component) -> void {
	component.current_position += (component.current_velocity * gse::system_clock::constant_update_time<time_step>()) + component.accumulators.position_correction;
}

auto gse::physics::update_rotation(motion_component& component) -> void {
	const auto alpha = component.accumulators.current_torque / component.moment_of_inertia;

	const auto delta_time = system_clock::constant_update_time<time_step>();

	component.angular_velocity += alpha * delta_time;
	component.accumulators.current_torque = {};

	const unitless::vec3 angular_velocity = component.angular_velocity.as<radians_per_second>();
	const quat omega_quaternion = { 0.f, angular_velocity.x(), angular_velocity.y(), angular_velocity.z() };

	// dQ = 0.5 * omega_quaternion * orientation
	const quat delta_quaternion = 0.5f * omega_quaternion * component.orientation;
	component.orientation += delta_quaternion * delta_time.as<seconds>();
	component.orientation = normalize(component.orientation);
}

auto gse::physics::update_bb(const motion_component& motion_component, collision_component& collision_component) -> void {
	collision_component.bounding_box.update(motion_component.current_position, motion_component.orientation);
}

auto gse::physics::update_object(motion_component& component, collision_component* collision_component) -> void {
	if (is_zero(component.current_velocity) && is_zero(component.accumulators.current_acceleration)) {
		component.moving = false;
	}
	else {
		component.moving = true;
	}

	if (component.position_locked) {
		return;
	}

	update_gravity(component);
	update_air_resistance(component);
	update_velocity(component);
	update_position(component);
	update_rotation(component);

	if (collision_component) {
		update_bb(component, *collision_component);
	}

	component.accumulators = {};
}