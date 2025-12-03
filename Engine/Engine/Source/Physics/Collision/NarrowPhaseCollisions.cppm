export module gse.physics:narrow_phase_collision;

import std;

import :motion_component;
import :bounding_box;
import :collision_component;
import gse.physics.math;
import gse.utility;

export namespace gse::narrow_phase_collision {
    auto resolve_collision(
        physics::motion_component* object_a,
        physics::collision_component& coll_a,
        physics::motion_component* object_b,
        const physics::collision_component& coll_b
    ) -> void;
}

namespace gse::narrow_phase_collision {
    static constexpr bool debug = false;
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
        const bounding_box& bounding_box,
        const unitless::vec3& dir
    ) -> vec3<length>;

    auto minkowski_difference(
        const bounding_box& bb1,
        const bounding_box& bb2,
        const unitless::vec3& dir
    ) -> minkowski_point;

    auto verify_portal_origin_containment(
        const minkowski_point& v0,
        const minkowski_point& v1,
        const minkowski_point& v2,
        const minkowski_point& v3
    ) -> bool;

    auto mpr_collision(
        const bounding_box& bb1,
        const bounding_box& bb2
    ) -> std::optional<mpr_result>;

    auto generate_contact_points(
        const bounding_box& bb1,
        const bounding_box& bb2,
        const unitless::vec3& collision_normal,
        length penetration_depth
    ) -> std::vector<vec3<length>>;
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_a, physics::collision_component& coll_a, physics::motion_component* object_b, const physics::collision_component& coll_b) -> void {
    if (!object_a || !object_b) {
        return;
    }

    auto res = mpr_collision(coll_a.bounding_box, coll_b.bounding_box);
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

    const length corrected_penetration = std::max(res->penetration - slop, length{ 0 });
    const vec3<length> correction = 0.01f * res->normal * corrected_penetration * percent;
    const float ratio_a = inv_mass_a / total_inv_mass;
    const float ratio_b = inv_mass_b / total_inv_mass;

    if (!object_a->position_locked) {
        object_a->accumulators.position_correction += correction * ratio_a;
    }
    if (!object_b->position_locked) {
        object_b->accumulators.position_correction -= correction * ratio_b;
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

auto gse::narrow_phase_collision::support_obb(const bounding_box& bounding_box, const unitless::vec3& dir) -> vec3<length> {
    vec3<length> result = bounding_box.center();
    const auto half_extents = bounding_box.half_extents();

    const auto obb = bounding_box.obb();

    for (int i = 0; i < 3; ++i) {
        float sign = dot(dir, obb.axes[i]) > 0 ? 1.0f : -1.0f;
        result += obb.axes[i] * half_extents[i] * sign;
    }

    return result;
}

auto gse::narrow_phase_collision::minkowski_difference(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& dir) -> minkowski_point {
    minkowski_point result{
        .support_a = support_obb(bb1, dir),
        .support_b = support_obb(bb2, -dir),
    };
    result.point = result.support_a - result.support_b;
    return result;
}

auto gse::narrow_phase_collision::mpr_collision(const bounding_box& bb1, const bounding_box& bb2) -> std::optional<mpr_result> {
    const length eps = meters(1e-4f);
    const auto eps2 = eps * eps;

    const auto obb1 = bb1.obb();
    const auto obb2 = bb2.obb();

    if constexpr (debug) {
        std::println("[MPR] ---- begin ----");
        std::println("[MPR] A center: {}, B center: {}", bb1.center(), bb2.center());
        std::println("[MPR] half extents A: {}, axes A[0..2]: {}, {}, {}", bb1.half_extents(), obb1.axes[0], obb1.axes[1], obb1.axes[2]);
        std::println("[MPR] half extents B: {}, axes B[0..2]: {}, {}, {}", bb2.half_extents(), obb2.axes[0], obb2.axes[1], obb2.axes[2]);
    }

    unitless::vec3 dir = normalize(bb1.center() - bb2.center());

    if (is_zero(dir)) {
        dir = { 1.0f, 0.0f, 0.0f };
    }

    minkowski_point v0 = minkowski_difference(bb1, bb2, dir);

    dir = normalize(-v0.point);

    minkowski_point v1 = minkowski_difference(bb1, bb2, dir);

    if (dot(v1.point, dir) <= 0.0f) {
        if constexpr (debug) {
            std::println("[MPR] ---- end (no collision: phase 1) ----");
        }
        return std::nullopt;
    }

    dir = normalize(cross(v0.point - v1.point, -v0.point));

    if (is_zero(dir)) {
        dir = normalize(cross(v0.point - v1.point, unitless::axis_y));
        if (is_zero(dir)) {
            dir = normalize(cross(v0.point - v1.point, unitless::axis_x));
        }
    }

    minkowski_point v2 = minkowski_difference(bb1, bb2, dir);

    if (dot(v2.point, dir) <= 0.0f) {
        if constexpr (debug) {
            std::println("[MPR] ---- end (no collision: phase 2) ----");
        }
        return std::nullopt;
    }

    dir = normalize(cross(v1.point - v0.point, v2.point - v0.point));

    if (dot(dir, -v0.point) < 0.0f) {
        dir = -dir;
    }

    minkowski_point v3 = minkowski_difference(bb1, bb2, dir);

    if (dot(v3.point, dir) <= 0.0f) {
        if constexpr (debug) {
            std::println("[MPR] ---- end (no collision: phase 3) ----");
        }
        return std::nullopt;
    }



    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        if constexpr (debug) {
            std::println("\n[MPR][iter {}] v0:{}, v1:{}, v2:{}, v3:{}", i, v0.point, v1.point, v2.point, v3.point);
        }

        unitless::vec3 n_a = normalize(cross(v1.point - v0.point, v2.point - v0.point));
        unitless::vec3 n_b = normalize(cross(v2.point - v0.point, v3.point - v0.point));
        unitless::vec3 n_c = normalize(cross(v3.point - v0.point, v1.point - v0.point));
        unitless::vec3 n_d = normalize(cross(v2.point - v1.point, v3.point - v1.point));

        length d_a = is_zero(n_a) ? meters(std::numeric_limits<float>::infinity()) : dot(n_a, v0.point);
        if (d_a < 0) {
            n_a = -n_a;
            d_a = -d_a;
        }
        length d_b = is_zero(n_b) ? meters(std::numeric_limits<float>::infinity()) : dot(n_b, v0.point);
        if (d_b < 0) {
            n_b = -n_b;
            d_b = -d_b;
        }
        length d_c = is_zero(n_c) ? meters(std::numeric_limits<float>::infinity()) : dot(n_c, v0.point);
        if (d_c < 0) {
            n_c = -n_c;
            d_c = -d_c;
        }
        length d_d = is_zero(n_d) ? meters(std::numeric_limits<float>::infinity()) : dot(n_d, v1.point);
        if (d_d < 0) {
            n_d = -n_d;
            d_d = -d_d;
        }

        if constexpr (debug) {
            std::println("[MPR][iter {}] d1:{}, d2:{}, d3:{}, d4:{}", i, d_a, d_b, d_c, d_d);
        }

        struct face {
            int id;
            unitless::vec3 n;
            length d;
        };

        face faces[4] = {
            { 0, n_a, d_a },
            { 1, n_b, d_b },
            { 2, n_c, d_c },
            { 3, n_d, d_d }
        };

        auto pick = [&](
            const bool require_positive
            ) -> face {
                length best = meters(std::numeric_limits<float>::infinity());
                int idx = -1;
                for (int k = 0; k < 4; ++k) {
                    if (is_zero(faces[k].n)) {
                        continue;
                    }
                    if (require_positive) {
                        if (faces[k].d <= eps) {
                            continue;
                        }
                    }
                    if (faces[k].d < best) {
                        best = faces[k].d;
                        idx = k;
                    }
                }
                if (idx < 0) {
                    best = length{ -1 };
                    for (int k = 0; k < 4; ++k) {
                        if (is_zero(faces[k].n)) {
                            continue;
                        }
                        if (faces[k].d > best) {
                            best = faces[k].d;
                            idx = k;
                        }
                    }
                }
                return faces[idx];
            };

        face choice = pick(true);
        if (choice.d == meters(std::numeric_limits<float>::infinity())) {
            choice = pick(false);
        }

        if constexpr (debug) {
            std::println("[MPR][iter {}] best_n: {}", i, choice.n);
        }

        auto replace_vertex = [&](
            const int face_id,
            const minkowski_point& p
            ) {
                switch (face_id) {
                case 0:  v3 = p; break;
                case 1:  v1 = p; break;
                case 2:  v2 = p; break;
                case 3:  v0 = p; break;
                default: v3 = p; break;
                }
            };

        auto vertex_point = [&](
            const int face_id
            ) -> const vec3<length>&{
                switch (face_id) {
                case 0:  return v3.point;
                case 1:  return v1.point;
                case 2:  return v2.point;
                case 3:  return v0.point;
                default: return v3.point;
                }
            };

        bool progressed = false;
        for (int attempt = 0; attempt < 4 && !progressed; ++attempt) {
            minkowski_point p = minkowski_difference(bb1, bb2, choice.n);
            if (const length projection_dist = dot(p.point, choice.n); projection_dist - choice.d < meters(1e-4f)) {
                
                const unitless::vec3 collision_normal = -choice.n;
                const length penetration_depth = choice.d;

            	if constexpr (debug) {
                    std::println("[MPR][iter {}] CONVERGED. penetration: {}", i, penetration_depth);
                }

                if constexpr (debug) {
                    if (penetration_depth >= meters(10.f)) {
                        std::vector<vec3<length>> face_vertices_obb_1;
                        std::array<vec3<length>, 4> point_cache;
                        for (size_t i = 0; i < 3; i++) {
                            point_cache = bb1.face_vertices(i);
                            for (size_t pt = 0; pt < 4; pt++) {
                                face_vertices_obb_1.emplace_back(point_cache[pt]);
                            }
                        }
                        std::vector<vec3<length>> face_vertices_obb_2;
                        for (size_t i = 0; i < 3; i++) {
                            point_cache = bb2.face_vertices(i);
                            for (size_t pt = 0; pt < 4; pt++) {
                                face_vertices_obb_2.emplace_back(point_cache[pt]);
                            }
                        }
                        obb obb_test1 = bb1.obb();
                        obb obb_test2 = bb2.obb();
                    }
                }
                std::vector<vec3<length>> contact_points = generate_contact_points(
                    bb1,
                    bb2,
                    collision_normal,
                    penetration_depth
                );

                vec3<length> final_contact_point = bb1.center();
                if (!contact_points.empty()) {
                    vec3<length> total_point;
                    for (const auto& point : contact_points) {
                        total_point += point;
                    }
                    final_contact_point = total_point / static_cast<float>(contact_points.size());
                }

                if constexpr (debug) {
                    std::println("[MPR] Contacts complete. Normal: {}, Penetration: {}", collision_normal, penetration_depth);
                    std::println("[MPR] ---- end (collision) ----");
                }

                return mpr_result{
                    .collided = true,
                    .normal = collision_normal,
                    .penetration = penetration_depth,
                    .contact_point = final_contact_point
                };
            }

            // before: const vec3<length>& opp = vertex_point(choice.id);
            //        if (const auto delta2 = dot(delta, delta); delta2 > eps2) { replace_vertex(...); }

            const vec3<length>& opp = vertex_point(choice.id);
            const auto delta2 = dot(p.point - opp, p.point - opp);

            if (delta2 > eps2) {
                // additional safety: ensure p is not equal to any *other* simplex vertex
                bool equals_other = false;
                auto equals_within_eps2 = [&](const vec3<length>& q) {
                    return dot(p.point - q, p.point - q) <= eps2;
                    };

                if (equals_within_eps2(v0.point) || equals_within_eps2(v1.point) ||
                    equals_within_eps2(v2.point) || equals_within_eps2(v3.point)) {
                    equals_other = true;
                }

                if (!equals_other) {
                    replace_vertex(choice.id, p);
                    progressed = true;
                }
                else {
                    // mark this face unusable and retry picking another face
                    faces[choice.id].d = meters(std::numeric_limits<float>::infinity());
                    if constexpr (debug) std::println("[MPR] p equals existing vertex -> skipping face {}", choice.id);
                    // pick a new face below in the loop (your existing code will do that)
                }
            }

            const auto delta = p.point - opp;


            if (/*const auto delta2 = dot(delta, delta);*/ delta2 > eps2) {
                replace_vertex(choice.id, p);
                progressed = true;
            }
            if (v1.point == v2.point || v1.point == v3.point || v1.point == v0.point ||
                v2.point == v3.point || v2.point == v0.point ||
                v3.point == v0.point) {
                // something went wrong, the simplex is degenerate
                if constexpr (debug) {
                    std::println("[MPR][iter {}] degenerate simplex -> retrying face {}", i, choice.id);
                }
                //faces[choice.id].d = meters(std::numeric_limits<float>::infinity());
                continue;
            }
            else {
                if (choice.id == 0) {
                    faces[0].d = meters(std::numeric_limits<float>::infinity());
                }
                if (choice.id == 1) {
                    faces[1].d = meters(std::numeric_limits<float>::infinity());
                }
                if (choice.id == 2) {
                    faces[2].d = meters(std::numeric_limits<float>::infinity());
                }
                if (choice.id == 3) {
                    faces[3].d = meters(std::numeric_limits<float>::infinity());
                }
                choice = pick(true);
                if (choice.d == meters(std::numeric_limits<float>::infinity())) {
                    choice = pick(false);
                }
                if constexpr (debug) {
                    std::println("[MPR][iter {}] retry face with best_n: {}", i, choice.n);
                }
            }
        }
    }

    if constexpr (debug) {
        std::println("[MPR][fail] no convergence after {} iters (eps: {}).", mpr_collision_refinement_iterations, eps);
        std::println("[MPR] ---- end (no collision) ----");
    }

    return std::nullopt;
}
auto gse::narrow_phase_collision::generate_contact_points(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& collision_normal, length penetration_depth) -> std::vector<vec3<length>> {

    struct face_info {
        std::array<vec3<length>, 4> vertices;
        unitless::vec3 normal;
    };

    // Reverting to your original logic since normals are confirmed correct in World Space
    auto find_best_face = [](const bounding_box& bb, const unitless::vec3& dir) -> face_info {
        float max_dot = -std::numeric_limits<float>::max();
        int best_face_idx = -1;

        const auto& normals = bb.face_normals();
        for (std::size_t i = 0; i < 6; ++i) {
            if (const float dot_product = dot(normals[i], dir); dot_product > max_dot) {
                max_dot = dot_product;
                best_face_idx = i;
            }
        }
        return { bb.face_vertices(best_face_idx), normals[best_face_idx] };
        };

    // 1. Identify Reference and Incident Faces
    const auto info1 = find_best_face(bb1, collision_normal);
    const auto info2 = find_best_face(bb2, -collision_normal);

    std::vector<vec3<length>> inc_face_poly;
    std::array<vec3<length>, 4> ref_face;
    unitless::vec3 ref_normal;

    // Determine which is reference
    if (dot(info1.normal, collision_normal) > dot(info2.normal, -collision_normal)) {
        ref_face = info1.vertices;
        inc_face_poly.assign(info2.vertices.begin(), info2.vertices.end());
        ref_normal = info1.normal;
    }
    else {
        ref_face = info2.vertices;
        inc_face_poly.assign(info1.vertices.begin(), info1.vertices.end());
        ref_normal = info2.normal;
    }

    // 2. Sutherland-Hodgman Clipping
    struct plane {
        unitless::vec3 normal;
        length distance;
    };

    auto clip = [](const std::vector<vec3<length>>& subject, const plane& p) -> std::vector<vec3<length>> {
        std::vector<vec3<length>> out;
        if (subject.empty()) return out;

        const auto* prev = &subject.back();
        length prev_dist = dot(p.normal, *prev) - p.distance;

        for (const auto& curr : subject) {
            length curr_dist = dot(p.normal, curr) - p.distance;

            // If crossing the plane
            if (prev_dist * curr_dist < 0.0f) {
                float t = prev_dist / (prev_dist - curr_dist);
                out.push_back(*prev + (curr - *prev) * t);
            }

            // If inside (positive distance)
            // Robustness: Use -epsilon to keep points that are effectively ON the line
            if (curr_dist >= -meters(1e-5f)) {
                out.push_back(curr);
            }

            prev = &curr;
            prev_dist = curr_dist;
        }
        return out;
        };

    // --- THE FIX IS HERE ---
    // Calculate the geometric center of the reference face.
    // We use this to force the side clipping planes to point INWARD.
    vec3<length> ref_center = { 0, 0, 0 };
    for (const auto& v : ref_face) ref_center += v;
    ref_center /= 4.0f;

    // Clip incident face against the side planes of the reference face
    for (size_t i = 0; i < 4; ++i) {
        if (inc_face_poly.empty()) break;

        const vec3<length>& v1 = ref_face[i];
        const vec3<length>& v2 = ref_face[(i + 1) % 4];

        const unitless::vec3 edge = normalize(v2 - v1);

        // Calculate normal. We don't know if this points In or Out yet
        // because we don't know the exact winding order of the source vertices.
        unitless::vec3 plane_normal = cross(ref_normal, edge);

        // ROBUSTNESS CHECK:
        // A valid side-plane for clipping MUST point towards the center of the polygon.
        vec3<length> to_center = ref_center - v1;

        // If the normal points away from the center (dot < 0), flip it.
        // This solves the "Returns Nothing" bug caused by winding order mismatches.
        if (dot(plane_normal, to_center) < 0.0f) {
            plane_normal = -plane_normal;
        }

        plane p{ plane_normal, dot(plane_normal, v1) };
        inc_face_poly = clip(inc_face_poly, p);
    }

    // 3. Filter and Project
    std::vector<vec3<length>> contacts;
    plane ref_plane{ ref_normal, dot(ref_normal, ref_face[0]) };

    for (const auto& v : inc_face_poly) {
        length dist = dot(ref_normal, v) - ref_plane.distance;

        // Keep points that are BEHIND the reference face (penetrating)
        // Note: dist is negative if the point is "behind" the normal.
        // We accept a small positive tolerance for floating point errors.
        if (dist <= meters(1e-3f)) {
            // Optional: Project point onto reference plane 
            // This flattens the manifold, which is usually better for physics stability.
            contacts.push_back(v - ref_normal * dist);
        }
    }

    return contacts;
}
//auto gse::narrow_phase_collision::generate_contact_points(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& collision_normal, length penetration_depth) -> std::vector<vec3<length>> {
//    auto find_best_face = [](const bounding_box& bb, const unitless::vec3& dir) -> std::array<vec3<length>, 4> {
//        float max_dot = -std::numeric_limits<float>::max();
//        int best_face_idx = -1;
//
//        for (std::size_t i = 0; i < 6; ++i) {
//            if (const float dot_product = dot(bb.face_normals()[i], dir); dot_product > max_dot) {
//                max_dot = dot_product;
//                best_face_idx = i;
//            }
//        }
//
//        return bb.face_vertices(best_face_idx);
//        };
//
//    const auto face1 = find_best_face(bb1, -collision_normal);
//    const auto face2 = find_best_face(bb2, collision_normal);
//
//    std::array<vec3<length>, 4> ref_face;
//    std::array<vec3<length>, 4> inc_face;
//    unitless::vec3 ref_normal;
//
//    const unitless::vec3 n1 = normalize(cross(face1[1] - face1[0], face1[2] - face1[0]));
//    const unitless::vec3 n2 = normalize(cross(face2[1] - face2[0], face2[2] - face2[0]));
//
//    if (dot(n1, -collision_normal) > dot(n2, collision_normal)) {
//        ref_face = face1;
//        inc_face = face2;
//        ref_normal = n1;
//    }
//    else {
//        ref_face = face2;
//        inc_face = face1;
//        ref_normal = n2;
//    }
//
//    struct plane {
//        unitless::vec3 normal;
//        length distance;
//    };
//
//    auto clip = [](const std::vector<vec3<length>>& subject_polygon, const plane& clip_plane) -> std::vector<vec3<length>> {
//        std::vector<vec3<length>> output_polygon;
//        if (subject_polygon.empty()) {
//            return output_polygon;
//        }
//
//        const auto* prev_v = &subject_polygon.back();
//        length prev_dist = dot(clip_plane.normal, *prev_v) - clip_plane.distance;
//
//        for (const auto& current_v : subject_polygon) {
//            const length current_dist = dot(clip_plane.normal, current_v) - clip_plane.distance;
//
//            if (prev_dist * current_dist < 0) {
//                const float t = prev_dist / (prev_dist - current_dist);
//                const vec3<length> intersection = *prev_v + (*prev_v - current_v) * -t;
//                output_polygon.push_back(intersection);
//            }
//
//            if (current_dist >= meters(0.f)) {
//                output_polygon.push_back(current_v);
//            }
//
//            prev_v = &current_v;
//            prev_dist = current_dist;
//        }
//
//        return output_polygon;
//        };
//
//    std::vector clipped_polygon(inc_face.begin(), inc_face.end());
//    for (size_t i = 0; i < ref_face.size(); ++i) {
//        if (clipped_polygon.empty()) {
//            break;
//        }
//
//        const vec3<length>& v1 = ref_face[i];
//        const vec3<length>& v2 = ref_face[(i + 1) % ref_face.size()];
//
//        const unitless::vec3 edge_vec = normalize(v2 - v1);
//        const unitless::vec3 plane_normal = cross(edge_vec, ref_normal);
//
//        plane p = {
//            .normal = plane_normal,
//            .distance = dot(plane_normal, v1)
//        };
//        clipped_polygon = clip(clipped_polygon, p);
//    }
//
//    if (clipped_polygon.empty()) {
//        return {};
//    }
//
//    std::vector<vec3<length>> contact_points;
//    const plane ref_plane = { ref_normal, dot(ref_normal, ref_face[0]) };
//
//    for (const auto& v : clipped_polygon) {
//        if (const length dist = dot(ref_normal, v) - ref_plane.distance; dist <= meters(0.01f)) {
//            contact_points.push_back(v - ref_normal * dist);
//        } 
//    }
//
//    return contact_points;
//}