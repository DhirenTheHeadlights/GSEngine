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
        const surface::property& surface
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

    component.sleeping = false;
    component.sleep_time = 0.f;
}

auto gse::physics::apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void {
    if (is_zero(force)) {
        return;
    }

    const auto delta_velocity = force * duration / std::max(component.mass, kilograms(0.0001f));

    component.current_velocity += delta_velocity;

    component.sleeping = false;
    component.sleep_time = 0.f;
}

auto gse::physics::update_friction(motion_component& component, const surface::property& surface) -> void {
    if (component.airborne) {
        return;
    }

    if (is_zero(component.current_velocity)) {
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
        update_friction(component, properties(surface::type::concrete));
    }
}

auto gse::physics::update_air_resistance(motion_component& component) -> void {
    for (int i = 0; i < 3; ++i) {
        const velocity v = component.current_velocity[i];
        if (v == meters_per_second(0.0f)) {
            continue;
        }

        constexpr density air_density = kilograms_per_cubic_meter(1.225f);
        const float drag_coefficient = component.airborne ? 0.47f : 1.05f;
        constexpr area cross_sectional_area = square_meters(1.0f);

        const force drag_force_magnitude = 0.5f * drag_coefficient * air_density * cross_sectional_area * v * v;
        const float direction = v > meters_per_second(0.f) ? -1.0f : 1.0f;

        auto drag_force = gse::vec3<force>(
            i == 0 ? drag_force_magnitude * direction : 0.0f,
            i == 1 ? drag_force_magnitude * direction : 0.0f,
            i == 2 ? drag_force_magnitude * direction : 0.0f
        );

        apply_force(component, drag_force, component.current_position);
    }
}

auto gse::physics::update_velocity(motion_component& component) -> void {
    const auto delta_time = gse::system_clock::constant_update_time<time_step>();

    if (component.self_controlled && !component.airborne) {
        constexpr float damping_factor = 5.0f;
        const float factor = std::max(0.f, 1.f - damping_factor * delta_time.as<seconds>());
        component.current_velocity *= factor;
    }

    component.current_velocity += component.accumulators.current_acceleration * delta_time;

    if (!component.airborne) {
        for (int i = 0; i < 3; ++i) {
	        if (const velocity v = component.current_velocity[i]; abs(v) < meters_per_second(0.01f)) {
                component.current_velocity[i] = meters_per_second(0.f);
            }
        }
    }

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
    if (!component.update_orientation) {
        return;
	}

    const auto alpha = component.accumulators.current_torque / component.moment_of_inertia;

    const auto delta_time = system_clock::constant_update_time<time_step>();

    component.angular_velocity += alpha * delta_time;

    constexpr float angular_damping = 1.0f;
    const float factor = std::max(0.f, 1.f - angular_damping * delta_time.as<seconds>());
    component.angular_velocity *= factor;

    if (constexpr angular_velocity max_angular_speed = radians_per_second(20.f); magnitude(component.angular_velocity) > max_angular_speed) {
        component.angular_velocity = normalize(component.angular_velocity) * max_angular_speed;
    }

    component.accumulators.current_torque = {};

    const unitless::vec3 angular_velocity_vec = component.angular_velocity.as<radians_per_second>();
    const quat omega_quaternion = { 0.f, angular_velocity_vec.x(), angular_velocity_vec.y(), angular_velocity_vec.z() };

    const quat delta_quaternion = 0.5f * omega_quaternion * component.orientation;
    component.orientation += delta_quaternion * delta_time.as<seconds>();
    component.orientation = normalize(component.orientation);
}

auto gse::physics::update_bb(const motion_component& motion_component, collision_component& collision_component) -> void {
    collision_component.bounding_box.update(motion_component.current_position, motion_component.orientation);
}

auto gse::physics::update_object(motion_component& component, collision_component* collision_component) -> void {
    const auto delta_time = gse::system_clock::constant_update_time<time_step>();

    if (component.position_locked) {
        component.current_velocity = { 0.f, 0.f, 0.f };
        component.angular_velocity = {};
        component.accumulators = {};
        component.moving = false;
        component.airborne = false;

        if (collision_component) {
            update_bb(component, *collision_component);
        }

        return;
    }

    if (component.sleeping && !component.self_controlled) {
        component.current_velocity = { 0.f, 0.f, 0.f };
        component.angular_velocity = {};
        component.accumulators = {};
        component.moving = false;

        if (collision_component) {
            update_bb(component, *collision_component);
        }

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

    const auto linear_speed = magnitude(component.current_velocity).as<meters_per_second>();
    const auto angular_speed = magnitude(component.angular_velocity).as<radians_per_second>();

    constexpr time linear_sleep_threshold = seconds(0.05f);
    constexpr time angular_sleep_threshold = seconds(0.1f);
    constexpr time sleep_time_threshold = seconds(0.5f);

    if (const bool can_try_sleep = component.can_sleep && !component.self_controlled && !component.airborne; can_try_sleep && linear_speed < linear_sleep_threshold && angular_speed < angular_sleep_threshold) {
        component.sleep_time += delta_time;
        if (component.sleep_time >= sleep_time_threshold) {
            component.sleeping = true;
            component.current_velocity = { 0.f, 0.f, 0.f };
            component.angular_velocity = {};
        }
    }
    else {
        component.sleep_time = 0.f;
        component.sleeping = false;
    }

    constexpr float linear_moving_threshold = 0.01f;
    constexpr float angular_moving_threshold = 0.05f;

    component.moving = linear_speed > linear_moving_threshold || angular_speed > angular_moving_threshold;
}
