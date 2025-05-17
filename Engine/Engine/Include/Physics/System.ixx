export module gse.physics.system;

import std;

import gse.physics.math;
import gse.core.object_registry;
import gse.core.main_clock;
import gse.graphics.render_component;
import gse.physics.surfaces;
import gse.physics.collision_component;
import gse.platform.glfw.input;
import gse.physics.motion_component;

export namespace gse::physics {
	auto apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position = { 0.f, 0.f, 0.f }) -> void;
	auto apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void;
	auto update_object(motion_component& component) -> void;
}

constexpr auto g_gravity = gse::vec3<gse::acceleration>(0.f, -9.8f, 0.f);

auto gse::physics::apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto acceleration = force / std::max(component.mass, kilograms(0.0001f));

	vec3<length> center_of_mass;

	if (const auto* render_component = gse::registry::get_component_ptr<gse::render_component>(component.parent_id); render_component) {
		center_of_mass = render_component->center_of_mass;
	}
	else {
		center_of_mass = component.current_position;
	}

	component.current_torque += cross(world_force_position - center_of_mass, force);
	component.current_acceleration += acceleration;
}

auto gse::physics::apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void {
	if (is_zero(force)) {
		return;
	}

	const auto delta_velocity = force * duration / std::max(component.mass, kilograms(0.0001f));

	component.current_velocity += delta_velocity;
}

auto update_friction(gse::physics::motion_component& component, const gse::surfaces::surface_properties& surface) -> void {
	if (component.airborne) {
		return;
	}

	const gse::force normal = component.mass * gse::magnitude(g_gravity);
	gse::force friction = normal * surface.friction_coefficient;

	if (component.self_controlled) {
		friction *= 5.f;
	}

	const gse::vec3 friction_force(-friction * gse::normalize(component.current_velocity));

	apply_force(component, friction_force, component.current_position);
}

auto update_gravity(gse::physics::motion_component& component) -> void {
	if (!component.affected_by_gravity) {
		return;
	}

	if (component.airborne) {
		const auto gravity_force = g_gravity * component.mass;
		apply_force(component, gravity_force, component.current_position);
	}
	else {
		component.current_acceleration.y = gse::max({ 0.f }, component.current_acceleration.y);
		update_friction(component, get_surface_properties(gse::surfaces::surface_type::concrete));
	}
}

auto update_air_resistance(gse::physics::motion_component& component) -> void {
	// Calculate drag force magnitude: F_d = 0.5 * C_d * rho * A * v^2, Units are in Newtons
	for (int i = 0; i < 3; ++i) {
		if (const gse::velocity velocity = component.current_velocity[i]; velocity != gse::meters_per_second(0.0f)) {
			constexpr float air_density = 1.225f;													// kg/m^3 (air density at sea level)
			const float drag_coefficient = component.airborne ? 0.47f : 1.05f;						// Approx for a sphere vs a box
			constexpr float cross_sectional_area = 1.0f;											// Example area in m^2, adjust according to the object

			const float drag_force_magnitude = (0.5f * drag_coefficient * air_density * cross_sectional_area * velocity * velocity).as_default_unit();
			const float direction = velocity > gse::meters_per_second(0) ? -1.0f : 1.0f;
			auto drag_force = gse::vec3<gse::force>(
				i == 0 ? drag_force_magnitude * direction : 0.0f,
				i == 1 ? drag_force_magnitude * direction : 0.0f,
				i == 2 ? drag_force_magnitude * direction : 0.0f
			);

			apply_force(component, drag_force, component.current_position);
		}
	}
}

auto update_velocity(gse::physics::motion_component& component) -> void {
	const auto delta_time = gse::main_clock::get_constant_update_time();

	if (component.self_controlled && !component.airborne) {
		constexpr float damping_factor = 5.0f;
		component.current_velocity *= std::max(0.f, 1.0f - damping_factor * delta_time.as_default_unit());
	}

	// Update current_velocity using the kinematic equation: v = v0 + at
	component.current_velocity += component.current_acceleration * delta_time;

	if (gse::magnitude(component.current_velocity) > component.max_speed && !component.airborne) {
		component.current_velocity = gse::normalize(component.current_velocity) * component.max_speed;
	}

	if (gse::is_zero(component.current_velocity)) {
		component.current_velocity = { 0.f, 0.f, 0.f };
	}

	component.current_acceleration = { 0.f, 0.f, 0.f };
}

auto update_position(gse::physics::motion_component& component) -> void {
	const auto delta_time = gse::main_clock::get_constant_update_time();

	// Update position using the kinematic equation: x = x0 + v0t + 0.5at^2
	component.current_position = component.current_velocity * delta_time + 0.5f * component.current_acceleration * delta_time * delta_time;
}

auto update_rotation(gse::physics::motion_component& component) -> void {
	const gse::time dt = gse::main_clock::get_constant_update_time();

	const auto alpha = component.current_torque / component.moment_of_inertia;

	component.angular_velocity += alpha * dt;

	component.current_torque = {};

	const gse::unitless::vec3 angular_velocity = component.angular_velocity.as<gse::units::radians_per_second>();
	const gse::quat omega_quaternion = { 0.f, angular_velocity.x, angular_velocity.y, angular_velocity.z };

	// dQ = 0.5 * omega_quaternion * orientation
	const gse::quat delta_quaternion = 0.5f * omega_quaternion * component.orientation;
	component.orientation += delta_quaternion * dt.as_default_unit();
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