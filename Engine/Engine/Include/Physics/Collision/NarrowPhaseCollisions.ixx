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

    coll_a.collision_information = {
        .colliding = true,
        .collision_normal = res->normal,
        .penetration = res->penetration,
        .collision_point = res->contact_point
    };

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
    static constexpr bool debug = true;
    const float eps_squared = 1e-8f;
    const length eps = meters(1e-4f);

    if constexpr (debug) {
        std::println("[MPR] ---- begin ----");
        std::println("[MPR] A center: {}, B center: {}", obb1.center, obb2.center);
        std::println("[MPR] half extents A: {}, axes A[0..2]: {}, {}, {}", obb1.half_extents(), obb1.axes[0], obb1.axes[1], obb1.axes[2]);
        std::println("[MPR] half extents B: {}, axes B[0..2]: {}, {}, {}", obb2.half_extents(), obb2.axes[0], obb2.axes[1], obb2.axes[2]);
    }

    auto dir = obb1.center - obb2.center;
	if (dir < vec3<length>{ eps_squared }) {
        dir = { 1.0f, 0.0f, 0.0f };
    }
    dir = normalize(dir);

    if constexpr (debug) {
        std::println("[MPR] init dir: {}", dir);
    }

    minkowski_point v0 = minkowski_difference(obb1, obb2, unitless::vec3(dir));
    if constexpr (debug) {
        std::println("[MPR] v0.point: {}  (sa: {}, sb: {})", v0.point, v0.support_a, v0.support_b);
    }

    dir = -v0.point;
    if (dir < vec3<length>{ eps_squared }) {
        dir = { 1.0f, 0.0f, 0.0f };
    }
    dir = normalize(dir);

    minkowski_point v1 = minkowski_difference(obb1, obb2, dir);

    if constexpr (debug) {
        const auto proj = dot(v1.point, dir);
        std::println("[MPR] v1.point: {}  dir: {}  v1·dir: {}", v1.point, dir, proj);
    }

    if (dot(v1.point, dir) <= 0) {
        if constexpr (debug) {
            std::println("[MPR][exit] origin is outside after v1 check (no overlap).");
            std::println("[MPR] ---- end (no collision) ----");
        }
        return std::nullopt;
    }

    dir = normalize(cross(v1.point - v0.point, -v0.point));
    minkowski_point v2 = minkowski_difference(obb1, obb2, dir);

    if constexpr (debug) {
        const auto proj2 = dot(v2.point, dir);
        std::println("[MPR] v2.point: {}  dir: {}  v2·dir: {}", v2.point, dir, proj2);
    }

    if (dot(v2.point, dir) <= 0) {
        if constexpr (debug) {
            std::println("[MPR][exit] origin is outside after v2 check (no overlap).");
            std::println("[MPR] ---- end (no collision) ----");
        }
        return std::nullopt;
    }

    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        unitless::vec3 v0v1 = (v1.point - v0.point).as<units::meters>();
        unitless::vec3 v0v2 = (v2.point - v0.point).as<units::meters>();
        unitless::vec3 n = normalize(cross(v0v1, v0v2));

        if constexpr (debug) {
            std::println("\n[MPR][iter {}] v0: {}, v1: {}, v2: {}", i, v0.point, v1.point, v2.point);
            std::println("[MPR][iter {}] portal n: {}", i, n);
        }

        if (dot(n, v0.point.as<units::meters>()) > 0.0f) {
            std::swap(v1, v2);
            n = -n;
            if constexpr (debug) {
                std::println("[MPR][iter {}] flipped portal.", i);
            }
        }

        minkowski_point v3 = minkowski_difference(obb1, obb2, n);
        const length plane_dist = -dot(n, v0.point.as<units::meters>());
        const length support_dist = dot(n, v3.point.as<units::meters>());
        const length gap = support_dist - plane_dist;

        if constexpr (debug) {
            std::println("[MPR][iter {}] v3: {}", i, v3.point);
            std::println("[MPR][iter {}] plane: {}, support: {}, gap: {} (eps: {})", i, -plane_dist, support_dist, gap, eps);
        }

        if (gap <= eps) {
            const length penetration = support_dist;
            if constexpr (debug) {
                std::println("[MPR][iter {}] CONVERGED by gap: penetration {}", i, penetration);
            }
            mpr_result out{
                .collided = true,
                .normal = n,
                .penetration = penetration
            };
            const auto q = (n * penetration).as<units::meters>();
            auto bary = barycentric(q, v0.point.as<units::meters>(), v1.point.as<units::meters>(), v2.point.as<units::meters>());
            out.contact_point = v0.support_a * bary[0] + v1.support_a * bary[1] + v2.support_a * bary[2];
            if constexpr (debug) {
                std::println("[MPR] contact bary: {}, {}, {}", bary[0], bary[1], bary[2]);
                std::println("[MPR] contact point: {}", out.contact_point);
                std::println("[MPR] ---- end (collision) ----");
            }
            return out;
        }

        unitless::vec3 ao = (-v0.point).as<units::meters>();
        unitless::vec3 ab_portal = (v1.point - v0.point).as<units::meters>();
        unitless::vec3 n_ab_plane = cross(ab_portal, n);

        if (dot(n_ab_plane, ao) > 0.0f) {
            if constexpr (debug) {
                std::println("[MPR][iter {}] refining portal (v0,v1 edge) -> replace v2 with v3.", i);
            }
            v2 = v3;
            continue;
        }

        unitless::vec3 ac_portal = (v2.point - v0.point).as<units::meters>();
        unitless::vec3 n_ac_plane = cross(n, ac_portal);

        if (dot(n_ac_plane, ao) > 0.0f) {
            if constexpr (debug) {
                std::println("[MPR][iter {}] refining portal (v2,v0 edge) -> replace v1 with v3.", i);
            }
            v1 = v3;
            continue;
        }

        if constexpr (debug) {
            std::println("[MPR][iter {}] refining portal (center region) -> replace v0 with v3.", i);
        }
        v0 = v3;
    }

    if constexpr (debug) {
        std::println("[MPR][fail] no convergence after {} iters (eps: {}).", mpr_collision_refinement_iterations, eps);
        std::println("[MPR] ---- end (no collision) ----");
    }
    return std::nullopt;
}