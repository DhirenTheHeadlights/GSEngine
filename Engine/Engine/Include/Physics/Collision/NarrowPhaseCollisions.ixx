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
        const oriented_bounding_box& obb2
    ) -> std::optional<mpr_result>;

    // A simple, self-contained test for the mpr_collision function.
    void run_mpr_self_test() {
        std::println("\n--- RUNNING MPR SELF-TEST ---");

        // Test Scenario: Two 1x1x1 cubes. One is at the origin, the other is
        // shifted by 0.5 units on the X-axis, creating a clear overlap.

        // 1. Define the Axis-Aligned Bounding Boxes first.
        axis_aligned_bounding_box aabb_a({ 0.5, 0, 0 }, { 1, 1, 1 });
        axis_aligned_bounding_box aabb_b({ 0, 0, 0 }, { 1, 1, 1 });

        // 2. Create Oriented Bounding Boxes from the AABBs.
        // Since they are axis-aligned, we use the default (identity) orientation.
        oriented_bounding_box obb_a(aabb_a);
        oriented_bounding_box obb_b(aabb_b);

        // 3. IMPORTANT: Update the axes after creation. Your API requires this.
        obb_a.update_axes();
        obb_b.update_axes();

        std::println("Testing Box A (center: {}) vs Box B (center: {})", obb_a.center, obb_b.center);

        // 4. Run the collision detection.
        auto result = mpr_collision(obb_a, obb_b);

        // 5. Check the results against the expected outcome.
        if (result) {
            std::println("SUCCESS: Collision detected.");

            // Expected results for this specific test case.
            const unitless::vec3 expected_normal = { -1.0f, 0.0f, 0.0f }; // Normal from B's perspective
            const length expected_penetration = meters(0.5f);

            std::println("   Normal: {} (Expected: {})", result->normal, expected_normal);
            std::println("   Penetration: {} (Expected: {})", result->penetration, expected_penetration);
            std::println("   Contact Point: {}", result->contact_point);

            // A simple check to see if the results are close to what we expect.
            if (distance(result->normal, expected_normal) < 0.01f && abs(result->penetration - expected_penetration) < 0.01f) {
                std::println("RESULT: The collision data looks correct!");
            }
            else {
                std::println("RESULT: The collision data is incorrect!");
            }
        }
        else {
            std::println("FAILURE: No collision was detected, but one was expected.");
        }
        std::println("--- SELF-TEST COMPLETE ---\n");
    }
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_a, physics::collision_component& coll_a, physics::motion_component* object_b, physics::collision_component& coll_b) -> void {
    run_mpr_self_test();

	if (!object_a || !object_b) {
        return;
    }

    auto res = mpr_collision(coll_a.obb, coll_b.obb);
    if (!res) {
        return;
    }

    if (dot(object_a->current_position - object_b->current_position, res->normal) < 0) {
        res->normal *= -1.0f;
    }

    coll_a.collision_information.colliding = true;
    coll_a.collision_information.collision_normal = res->normal;
    coll_a.collision_information.penetration = res->penetration;
    coll_a.collision_information.collision_point = res->contact_point;

    const inverse_mass inv_mass_a = object_a->position_locked ? inverse_mass{ 0 } : 1.0f / object_a->mass;
    const inverse_mass inv_mass_b = object_b->position_locked ? inverse_mass{ 0 } : 1.0f / object_b->mass;
    const inverse_mass total_inv_mass = inv_mass_a + inv_mass_b;

    if (total_inv_mass <= inverse_mass{ 0 }) {
        return; 
    }

    constexpr length slop = meters(0.01f);
    constexpr float percent = 0.8f; 

    const length corrected_penetration = max(res->penetration - slop, length{ 0 });
    const vec3<length> correction = res->normal * corrected_penetration * percent;

    const float ratio_a = inv_mass_a / total_inv_mass;
    const float ratio_b = inv_mass_b / total_inv_mass;

    if (!object_a->position_locked) {
        object_a->current_position += correction * ratio_a;
    }
    if (!object_b->position_locked) {
        object_b->current_position -= correction * ratio_b;
    }

    if (res->normal.y() > 0.7f) {
        object_a->airborne = false;
    }
    if (res->normal.y() < -0.7f) {
        object_b->airborne = false;
    }

    const auto r_a = res->contact_point - object_a->current_position; 
    const auto r_b = res->contact_point - object_b->current_position; 
    
    const auto vel_a = object_a->current_velocity + cross(object_a->angular_velocity, r_a);
    const auto vel_b = object_b->current_velocity + cross(object_b->angular_velocity, r_b);
    const auto relative_velocity = vel_a - vel_b; 

    const velocity vel_along_normal = dot(relative_velocity, res->normal);

    if (vel_along_normal > velocity{ 0 }) {
        return;
    }

    constexpr float restitution = 0.5f; 

    const physics::inv_inertia_mat inv_i_a = object_a->position_locked ? physics::inv_inertia_mat(0.0f) : object_a->inv_inertial_tensor();
    const physics::inv_inertia_mat inv_i_b = object_b->position_locked ? physics::inv_inertia_mat(0.0f) : object_b->inv_inertial_tensor();

    const auto rcross_a_part = cross(inv_i_a * cross(r_a, res->normal), r_a);
    const auto rcross_b_part = cross(inv_i_b * cross(r_b, res->normal), r_b);

    const inverse_mass denom = total_inv_mass + dot(rcross_a_part, res->normal) + dot(rcross_b_part, res->normal);
    const auto jn = -(1.0f + restitution) * vel_along_normal / denom;

    const auto impulse_vec = res->normal * jn; 

    if (!object_a->position_locked) {
        object_a->current_velocity += impulse_vec * inv_mass_a;
        object_a->angular_velocity += inv_i_a * cross(r_a, impulse_vec);
    }
    if (!object_b->position_locked) {
        object_b->current_velocity -= impulse_vec * inv_mass_b;
        object_b->angular_velocity -= inv_i_b * cross(r_b, impulse_vec);
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

auto gse::narrow_phase_collision::mpr_collision(const oriented_bounding_box& obb1, const oriented_bounding_box& obb2) -> std::optional<mpr_result> {
    unitless::vec3 dir = normalize(obb2.center - obb1.center);
    if (is_zero(dir)) {
        dir = { 1, 0, 0 };
    }
    minkowski_point v0 = minkowski_difference(obb1, obb2, dir);

    dir = normalize(-v0.point);
    minkowski_point v1 = minkowski_difference(obb1, obb2, dir);

    if (dot(v1.point, dir) <= length{ 0 }) {
        return std::nullopt;
    }

    const auto v1_v0 = v1.point - v0.point;
    const auto v0_origin = -v0.point;
    unitless::vec3 n_plane = cross(v1_v0, v0_origin).as<units::meters>();

    if (is_zero(n_plane)) {
        dir = cross(v1_v0, vec3<length>{ 1.f, 0.f, 0.f }).as<units::meters>();
        if (is_zero(dir)) {
            dir = cross(v1_v0, vec3<length>{ 0.f, 0.f, 1.f }).as<units::meters>();
        }
    }
    else {
        dir = cross(n_plane, v1_v0.as<units::meters>());
    }
    dir = normalize(dir);

    minkowski_point v2 = minkowski_difference(obb1, obb2, dir);

    if (dot(v2.point.as<units::meters>(), dir) <= 0) {
        return std::nullopt;
    }

    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        unitless::vec3 n = normalize(cross(v1.point - v0.point, v2.point - v0.point));

        if (dot(n, v0.point.as<units::meters>()) >= 0) {
            std::swap(v1, v2);
            n = -n;
        }

        // --- START DEBUGGING CODE ---
        std::println("\nIter {}:", i);
        std::println("    Portal Normal (n): {}", n);
        // --- END DEBUGGING CODE ---

        minkowski_point v3 = minkowski_difference(obb1, obb2, n);
        const length penetration = dot(v3.point, n);

        const auto termination_value = dot((v3.point - v0.point), n);

        // --- START DEBUGGING CODE ---
        std::println("    Termination Value: {} (Target: <= {})", termination_value, meters(1e-4));
        // --- END DEBUGGING CODE ---

        if (termination_value <= meters(1e-4)) {
            // --- START DEBUGGING CODE ---
            std::println("SUCCESS: Converged at iteration {}.", i);
            // --- END DEBUGGING CODE ---
            mpr_result out = {
                .collided = true,
                .normal = n,
                .penetration = penetration
            };
            auto bary = barycentric(
                (n * penetration).as<units::meters>(),
                v0.point.as<units::meters>(),
                v1.point.as<units::meters>(),
                v2.point.as<units::meters>()
            );
            out.contact_point =
                v0.support_a * bary[0] +
                v1.support_a * bary[1] +
                v2.support_a * bary[2];
            return out;
        }

        // Normal for face (v0, v1, v3), pointing outwards
        unitless::vec3 n1 = cross(v3.point - v0.point, v1.point - v0.point).as<units::meters>();
        if (dot(n1, -v0.point) >= 0) {
            v2 = v3;
            continue;
        }

        // Normal for face (v1, v2, v3), pointing outwards
        unitless::vec3 n2 = cross(v3.point - v1.point, v2.point - v1.point).as<units::meters>();
        if (dot(n2, -v1.point) >= 0) {
            v0 = v3;
            continue;
        }

        // Normal for face (v2, v0, v3), pointing outwards
        unitless::vec3 n3 = cross(v3.point - v2.point, v0.point - v2.point).as<units::meters>();
        if (dot(n3, -v2.point) >= 0) {
            v1 = v3;
            continue;
        }

        mpr_result out = {
            .collided = true,
            .normal = n,
            .penetration = penetration
        };
        auto bary = barycentric(
            (n * penetration).as<units::meters>(),
            v0.point.as<units::meters>(),
            v1.point.as<units::meters>(),
            v2.point.as<units::meters>()
        );
        out.contact_point =
            v0.support_a * bary[0] +
            v1.support_a * bary[1] +
            v2.support_a * bary[2];
        return out;
    }

    // --- START DEBUGGING CODE ---
    std::println("FAILURE: Loop finished without converging after {} iterations.", mpr_collision_refinement_iterations);
    // --- END DEBUGGING CODE ---
    return std::nullopt;
}