export module gse.physics:motion_component;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::physics {
    struct motion_component {
        [[= networked]] vec3<current_position> current_position;
        [[= networked]] vec3<velocity> current_velocity;
        [[= networked]] mass mass = kilograms(1.f);
        [[= networked]] quat orientation = quat(1.f, 0.f, 0.f, 0.f);
        [[= networked]] vec3<angular_velocity> angular_velocity;
        [[= networked]] inertia moment_of_inertia = kilograms_meters_squared(163.f);
        [[= networked]] bool affected_by_gravity = true;
        [[= networked]] bool airborne = true;
        [[= networked]] bool position_locked = false;

        float restitution = 0.3f;
        bool sleeping = false;
        bool update_orientation = true;

        vec3<velocity> velocity_drive_target;
        bool velocity_drive_active = false;

        vec3<impulse> pending_impulse;
    };

    auto transformation_matrix(
        const motion_component& mc
    ) -> mat4f;

    auto inv_inertial_tensor(
        const motion_component& mc
    ) -> mat3<inverse_inertia>;
}

auto gse::physics::transformation_matrix(const motion_component& mc) -> mat4f {
    const mat4f translation = translate(mat4f(1.0f), mc.current_position);
    const auto rotation = mat4f(mat3_cast(mc.orientation));
    return translation * rotation;
}

auto gse::physics::inv_inertial_tensor(const motion_component& mc) -> mat3<inverse_inertia> {
    const inverse_inertia inv_i = 1.f / mc.moment_of_inertia;
    const auto inv_i_body = gse::identity<float, 3, 3>() * inv_i;
    const auto rotation = mat3_cast(mc.orientation);
    return rotation * inv_i_body * rotation.transpose();
}
