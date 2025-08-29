export module gse.physics:narrow_phase_collisions;

import std;

import :motion_component;
import :bounding_box;
import :collision_component;

import gse.physics.math;

export namespace gse::narrow_phase_collision {
    auto resolve_collision(
        physics::motion_component* object_a,
        physics::collision_component& coll_a,
        physics::motion_component* object_b,
        physics::collision_component& coll_b
    ) -> void;
}

namespace gse::narrow_phase_collision {
    constexpr int mpr_collision_refinement_iterations = 32;

    struct mpr_result {
        bool collided = false;
        unitless::vec3 normal;
        length penetration;
        vec3<length> contact_point;
    };

    struct minkowski_point {
        vec3<length> point;
        vec3<length> support_a;
        vec3<length> support_b;
    };

    auto support_obb(
        const oriented_bounding_box& obb,
        const unitless::vec3& dir
    ) -> vec3<length>;

    auto minkowski_difference(
        const oriented_bounding_box& obb1,
        const oriented_bounding_box& obb2, 
        const unitless::vec3& dir
    ) -> minkowski_point;

    auto mpr_collision(
        const oriented_bounding_box& obb1,
        const oriented_bounding_box& obb2,
        mpr_result& out
    ) -> bool;
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_a, physics::collision_component& coll_a, physics::motion_component* object_b, physics::collision_component& coll_b) -> void {
    if (!object_a || !object_b) {
        return;
    }

    mpr_result res;
    if (!mpr_collision(coll_a.oriented_bounding_box, coll_b.oriented_bounding_box, res)) {
        return;
    }

    if (gse::dot((object_a->current_position - object_b->current_position).as<units::meters>(), res.normal) < 0.0f) {
        res.normal *= -1.0f;
    }

    coll_a.collision_information.colliding = true;
    coll_a.collision_information.collision_normal = res.normal;
    coll_a.collision_information.penetration = res.penetration;
    coll_a.collision_information.collision_point = res.contact_point;

    const float inv_mass_a = object_a->position_locked ? 0.0f : 1.0f / object_a->mass.as<units::kilograms>();
    const float inv_mass_b = object_b->position_locked ? 0.0f : 1.0f / object_b->mass.as<units::kilograms>();
    const float total_inv_mass = inv_mass_a + inv_mass_b;

    if (total_inv_mass <= 0.0f) {
        return; // Both objects are static, nothing to do.
    }

    constexpr float slop = 0.01f;
    constexpr float percent = 0.8f;
    const float corrected_penetration = std::max(res.penetration.as_default_unit() - slop, 0.0f);
    const auto correction = res.normal * (corrected_penetration / total_inv_mass) * percent;

    if (!object_a->position_locked) {
        object_a->current_position += gse::vec3<length>(correction * inv_mass_a);
    }
    if (!object_b->position_locked) {
        object_b->current_position -= gse::vec3<length>(correction * inv_mass_b);
    }

    if (res.normal.y() > 0.7f) {
        object_a->airborne = false;
    }
    if (res.normal.y() < -0.7f) {
        object_b->airborne = false;
    }

    const auto r_a = res.contact_point - object_a->current_position;
    const auto r_b = res.contact_point - object_b->current_position;

    const auto vel_a = object_a->current_velocity.as<units::meters_per_second>() + cross(object_a->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
    const auto vel_b = object_b->current_velocity.as<units::meters_per_second>() + cross(object_b->angular_velocity.as<units::radians_per_second>(), r_b.as<units::meters>());
    const auto relative_velocity = vel_a - vel_b;
    const float vel_along_normal = dot(relative_velocity, res.normal);

    if (vel_along_normal > 0.0f) {
        return;
    }

    constexpr float restitution = 0.5f;

    const mat3 inv_i_a = object_a->position_locked ? mat3(0.0f) : object_a->inv_inertial_tensor();
    const mat3 inv_i_b = object_b->position_locked ? mat3(0.0f) : object_b->inv_inertial_tensor();

    const auto rcross_a_part = cross(inv_i_a * cross(r_a.as<units::meters>(), res.normal), r_a.as<units::meters>());
    const auto rcross_b_part = cross(inv_i_b * cross(r_b.as<units::meters>(), res.normal), r_b.as<units::meters>());

    const float denom = total_inv_mass + dot(rcross_a_part, res.normal) + dot(rcross_b_part, res.normal);

    float jn = -(1.0f + restitution) * vel_along_normal;
    jn /= denom;

    const auto impulse = res.normal * jn;

    if (!object_a->position_locked) {
        object_a->current_velocity += gse::vec3<velocity>(impulse * inv_mass_a);
        object_a->angular_velocity += gse::vec3<angular_velocity>(inv_i_a * cross(r_a.as<units::meters>(), impulse));
    }
    if (!object_b->position_locked) {
        object_b->current_velocity -= gse::vec3<velocity>(impulse * inv_mass_b);
        object_b->angular_velocity -= gse::vec3<angular_velocity>(inv_i_b * cross(r_b.as<units::meters>(), impulse));
    }
}

auto gse::narrow_phase_collision::support_obb(const oriented_bounding_box& obb, const unitless::vec3& dir) -> vec3<length> {
    vec3<length> result = obb.center;
    const auto half_extents = obb.half_extents();

    for (int i = 0; i < 3; ++i) {
        float sign = dot(dir, obb.axes[i]) > 0 ? 1.0f : -1.0f;
        result += obb.axes[i] * half_extents[i] * sign;
    }
    return result;
}

auto gse::narrow_phase_collision::minkowski_difference(const oriented_bounding_box& obb1, const oriented_bounding_box& obb2, const unitless::vec3& dir) -> minkowski_point {
    minkowski_point result{
        .support_a = support_obb(obb1, dir),
        .support_b = support_obb(obb2, -dir),
    };
    result.point = result.support_a - result.support_b;
    return result;
}

auto gse::narrow_phase_collision::mpr_collision(const oriented_bounding_box& obb1, const oriented_bounding_box& obb2, mpr_result& out) -> bool {
    minkowski_point v0;
    v0.support_a = obb1.center;
    v0.support_b = obb2.center;
    v0.point = v0.support_a - v0.support_b;

    if (is_zero(v0.point)) {
        v0.point = gse::vec3<length>{ 0.0001, 0, 0 };
    }

    unitless::vec3 n = normalize(-v0.point);
    minkowski_point v1 = minkowski_difference(obb1, obb2, n);

    if (dot(v1.point.as<units::meters>(), n) <= 0) {
        return false;
    }

    n = gse::normalize(cross(cross(v0.point - v1.point, -v0.point), v0.point - v1.point));
    minkowski_point v2 = minkowski_difference(obb1, obb2, n);

    if (dot(v2.point.as<units::meters>(), n) <= 0) {
        return false;
    }

    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        n = gse::normalize(cross(v1.point - v0.point, v2.point - v0.point));

        if (dot(n, v0.point.as<units::meters>()) > 0) {
            std::swap(v1, v2);
            n = -n;
        }

        minkowski_point v3 = minkowski_difference(obb1, obb2, n);

        if (dot(v3.point.as<units::meters>(), n) <= dot(v0.point.as<units::meters>(), n) + 0.0001f) {
            out = {
                .collided = true,
                .normal = n,
                .penetration = dot(v3.point.as<units::meters>(), n)
            };

            auto bary = barycentric(
                (n * out.penetration).as<units::meters>(),
                v0.point.as<units::meters>(),
                v1.point.as<units::meters>(),
                v2.point.as<units::meters>()
            );

            out.contact_point =
                v0.support_a * bary[0] +
                v1.support_a * bary[1] +
                v2.support_a * bary[2];

            return true;
        }

        if (gse::dot(cross((v1.point - v0.point).as<units::meters>(), (v3.point - v0.point).as<units::meters>()), v0.point.as<units::meters>()) < 0) {
            v2 = v3;
        }
        else if (gse::dot(cross((v3.point - v0.point).as<units::meters>(), (v2.point - v0.point).as<units::meters>()), v0.point.as<units::meters>()) < 0) {
            v1 = v3;
        }
        else {
            v0 = v3;
        }
    }

    return false;
}


