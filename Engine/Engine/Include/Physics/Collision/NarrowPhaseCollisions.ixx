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
    const length eps = meters(1e-4f);

    if constexpr (debug) {
        std::println("[MPR] ---- begin ----");
        std::println("[MPR] A center: {}, B center: {}", obb1.center, obb2.center);
        std::println("[MPR] half extents A: {}, axes A[0..2]: {}, {}, {}", obb1.half_extents(), obb1.axes[0], obb1.axes[1], obb1.axes[2]);
        std::println("[MPR] half extents B: {}, axes B[0..2]: {}, {}, {}", obb2.half_extents(), obb2.axes[0], obb2.axes[1], obb2.axes[2]);
    }

    unitless::vec3 dir = normalize(obb2.center - obb1.center);
    if (is_zero(dir)) {
        dir = { 1.f, 0.f, 0.f };
    }
    if constexpr (debug) {
        std::println("[MPR] init dir: {}", dir);
    }

    minkowski_point v0 = minkowski_difference(obb1, obb2, dir);
    if constexpr (debug) {
        std::println("[MPR] v0.point: {}  (sa: {}, sb: {})", v0.point, v0.support_a, v0.support_b);
    }

    dir = normalize(-v0.point);
    minkowski_point v1 = minkowski_difference(obb1, obb2, dir);

    if constexpr (debug) {
        const length proj = dot(v1.point, dir);
        std::println("[MPR] v1.point: {}  dir: {}  v1·dir: {}", v1.point, dir, proj);
    }

    if (dot(v1.point, dir) <= length{ 0 }) {
        if constexpr (debug) {
            std::println("[MPR][exit] origin is outside after v1 check (no overlap).");
            std::println("[MPR] ---- end (no collision) ----");
        }
        return std::nullopt;
    }

    const unitless::vec3 ab0 = (v1.point - v0.point).as<units::meters>();
    const unitless::vec3 ao0 = (-v0.point).as<units::meters>();
    unitless::vec3 n_plane = cross(ab0, ao0);

    if (is_zero(n_plane)) {
        dir = normalize(cross(ab0, unitless::vec3{ 1.f, 0.f, 0.f }));
        if (is_zero(dir)) {
            dir = normalize(cross(ab0, unitless::vec3{ 0.f, 0.f, 1.f }));
        }
        if constexpr (debug) {
            std::println("[MPR] n_plane degenerate; picked orthogonal dir: {}", dir);
        }
    }
    else {
        dir = normalize(cross(n_plane, ab0));
        if constexpr (debug) {
            std::println("[MPR] n_plane: {}  dir: {}", n_plane, dir);
        }
    }

    minkowski_point v2 = minkowski_difference(obb1, obb2, dir);

    if constexpr (debug) {
        const length proj2 = dot(v2.point.as<units::meters>(), dir);
        std::println("[MPR] v2.point: {}  dir: {}  v2·dir: {}", v2.point, dir, proj2);
    }

    if (dot(v2.point.as<units::meters>(), dir) <= length{ 0 }) {
        if constexpr (debug) {
            std::println("[MPR][exit] origin is outside after v2 check (no overlap).");
            std::println("[MPR] ---- end (no collision) ----");
        }
        return std::nullopt;
    }

    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        unitless::vec3 ab = (v1.point - v0.point).as<units::meters>();
        unitless::vec3 ac = (v2.point - v0.point).as<units::meters>();
        unitless::vec3 n = normalize(cross(ab, ac));

        if constexpr (debug) {
            std::println("\n[MPR][iter {}] v0: {}, v1: {}, v2: {}", i, v0.point, v1.point, v2.point);
            std::println("[MPR][iter {}] ab: {}, ac: {}, portal n: {}", i, ab, ac, n);
        }

        // Ensure portal faces the origin
        const length face_dot = dot(n, v0.point.as<units::meters>());
        if (face_dot > length{ 0 }) {
            std::swap(v1, v2);
            n = -n;

            // recompute ab/ac for logs
            ab = (v1.point - v0.point).as<units::meters>();
            ac = (v2.point - v0.point).as<units::meters>();

            if constexpr (debug) {
                std::println("[MPR][iter {}] flipped portal (face_dot was {} > 0).", i, face_dot);
                std::println("[MPR][iter {}] new ab: {}, new ac: {}, new n: {}", i, ab, ac, n);
            }
        }

        minkowski_point v3 = minkowski_difference(obb1, obb2, n);

        const length plane_dist = dot(v0.point.as<units::meters>(), n);
        const length support_dist = dot(v3.point.as<units::meters>(), n);
        const length gap = support_dist - plane_dist;

        if constexpr (debug) {
            std::println("[MPR][iter {}] v3: {}", i, v3.point);
            std::println("[MPR][iter {}] plane_dist: {}, support_dist: {}, gap: {} (eps: {})", i, plane_dist, support_dist, gap, eps);
        }

        if (gap <= eps) {
            const length penetration = support_dist;

            if constexpr (debug) {
                std::println("[MPR][iter {}] CONVERGED: penetration {}", i, penetration);
            }

            mpr_result out{
                .collided = true,
                .normal = n,
                .penetration = penetration
            };

            const auto q = (n * penetration).as<units::meters>();
            auto bary = barycentric(
                q,
                v0.point.as<units::meters>(),
                v1.point.as<units::meters>(),
                v2.point.as<units::meters>()
            );

            out.contact_point =
                v0.support_a * bary[0] +
                v1.support_a * bary[1] +
                v2.support_a * bary[2];

            if constexpr (debug) {
                std::println("[MPR] contact bary: {}, {}, {}", bary[0], bary[1], bary[2]);
                std::println("[MPR] contact point: {}", out.contact_point);
                std::println("[MPR] ---- end (collision) ----");
            }

            return out;
        }

        // Side-plane tests (all measured toward origin)
        const unitless::vec3 n_ab = cross(ab, n);
        const unitless::vec3 bc = (v2.point - v1.point).as<units::meters>();
        const unitless::vec3 n_bc = cross(bc, n);
        const unitless::vec3 ca = (v0.point - v2.point).as<units::meters>();
        const unitless::vec3 n_ca = cross(ca, n);

        const float d_ab = dot(n_ab, (-v0.point).as<units::meters>());
        const float d_bc = dot(n_bc, (-v1.point).as<units::meters>());
        const float d_ca = dot(n_ca, (-v2.point).as<units::meters>());

        if constexpr (debug) {
            std::println("[MPR][iter {}] side tests -> d_ab: {}, d_bc: {}, d_ca: {}", i, d_ab, d_bc, d_ca);
        }

        if (d_ab > 0.f) {
            if constexpr (debug) {
                std::println("[MPR][iter {}] replace v2 with v3 (outside AB).", i);
            }
            v2 = v3;
            continue;
        }

        if (d_bc > 0.f) {
            if constexpr (debug) {
                std::println("[MPR][iter {}] replace v0 with v3 (outside BC).", i);
            }
            v0 = v3;
            continue;
        }

        if (d_ca > 0.f) {
            if constexpr (debug) {
                std::println("[MPR][iter {}] replace v1 with v3 (outside CA).", i);
            }
            v1 = v3;
            continue;
        }

        // If we’re inside all three, we should already have converged by gap;
        // fall back to replacing the longest edge’s opposite for progress.
        if constexpr (debug) {
            std::println("[MPR][iter {}] inside all side-planes; fallback replace v1 with v3.", i);
        }
        v1 = v3;
    }

    if constexpr (debug) {
        std::println("[MPR][fail] no convergence after {} iters (eps: {}).", mpr_collision_refinement_iterations, eps);
        // Best-effort summary of final portal
        // (Recompute last values for context if needed in your call site.)
        std::println("[MPR] ---- end (no collision) ----");
    }
    return std::nullopt;
}