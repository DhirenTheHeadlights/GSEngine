export module gse.physics:motion_component;

import std;

import gse.utility;
import gse.math;

export namespace gse::physics {
    struct motion_component_net {
        vec3<length> current_position;
        vec3<velocity> current_velocity;

        mass mass = kilograms(1.f);

        quat orientation = quat(1.f, 0.f, 0.f, 0.f);
        vec3<angular_velocity> angular_velocity;
        inertia moment_of_inertia = kilograms_meters_squared(1.f);

        bool affected_by_gravity = true;
        bool airborne = true;
        bool position_locked = false;
    };

    struct motion_component_data {
        vec3<length> current_position;
        vec3<velocity> current_velocity;

        mass mass = kilograms(1.f);

        quat orientation = quat(1.f, 0.f, 0.f, 0.f);
        vec3<angular_velocity> angular_velocity;
        inertia moment_of_inertia = kilograms_meters_squared(163.f);

        float restitution = 0.3f;

        bool affected_by_gravity = true;
        bool airborne = true;
        bool position_locked = false;
        bool can_sleep = true;
        bool sleeping = false;
		bool update_orientation = true;

        vec3<length> previous_position;
        quat previous_orientation = quat(1.f, 0.f, 0.f, 0.f);
        vec3<length> render_position;
        quat render_orientation = quat(1.f, 0.f, 0.f, 0.f);

        vec3<velocity> velocity_drive_target;
        bool velocity_drive_active = false;

        vec3<velocity> pending_impulse;
    };

    struct motion_component : component<motion_component_data, motion_component_net> {
        using component::component;

        auto transformation_matrix() const -> mat4f;
        auto inv_inertial_tensor() const -> mat3<inverse_inertia>;
    };
}

auto gse::physics::motion_component::transformation_matrix() const -> mat4f {
    const mat4f translation = translate(mat4f(1.0f), current_position);
    const auto rotation = mat4f(mat3_cast(orientation));
    return translation * rotation;
}

auto gse::physics::motion_component::inv_inertial_tensor() const -> mat3<inverse_inertia> {
    const float i_body = moment_of_inertia.as<kilograms_meters_squared>();
    const mat3f inv_i_body_raw = gse::identity<float, 3, 3>() * (1.f / i_body);
    const auto rotation = mat3_cast(orientation);
    const mat3f result_raw = rotation * inv_i_body_raw * rotation.transpose();
    return mat3<inverse_inertia>(result_raw);
}
