export module gse.physics:narrow_phase_collision;

import std;

import :motion_component;
import :bounding_box;
import :collision_component;
import :contact_manifold;
import gse.math;
import gse.utility;

export namespace gse::narrow_phase_collision {
    struct sat_result {
        unitless::vec3 normal;
        length separation;
        bool is_speculative;
    };

    auto resolve_collision(
        physics::motion_component* object_a,
        physics::collision_component& coll_a,
        physics::motion_component* object_b,
        physics::collision_component& coll_b
    ) -> void;

    auto sat_speculative(
        const bounding_box& bb1,
        const bounding_box& bb2,
        length speculative_margin
    ) -> std::optional<sat_result>;

    auto generate_manifold(
        const bounding_box& bb1,
        const bounding_box& bb2,
        const unitless::vec3& normal,
        length separation
    ) -> contact_manifold;
}

namespace gse::narrow_phase_collision {
    constexpr int mpr_collision_refinement_iterations = 32;

    struct mpr_result {
        bool collided = false;
        unitless::vec3 normal;
        length penetration;
        std::vector<vec3<length>> contact_points;
    };

    struct minkowski_point {
        vec3<length> point;
        vec3<length> support_a;
        vec3<length> support_b;
    };

    auto support_obb(
        const bounding_box& bb,
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
	    const unitless::vec3& collision_normal
    ) -> std::vector<vec3<length>>;

    auto sat_penetration(const bounding_box& bb1,
        const bounding_box& bb2
    ) -> std::pair<unitless::vec3, length>;
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_a, physics::collision_component& coll_a, physics::motion_component* object_b, physics::collision_component& coll_b) -> void {
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

    if (res->contact_points.empty()) {
        return;
    }

    coll_a.collision_information = {
        .colliding = true,
        .collision_normal = res->normal,
        .penetration = res->penetration,
        .collision_points = res->contact_points
    };

    coll_b.collision_information = {
        .colliding = true,
        .collision_normal = -res->normal,
        .penetration = res->penetration,
        .collision_points = res->contact_points
    };

    const inverse_mass inv_mass_a = object_a->position_locked ? inverse_mass{ 0 } : 1.0f / object_a->mass;
    const inverse_mass inv_mass_b = object_b->position_locked ? inverse_mass{ 0 } : 1.0f / object_b->mass;
    const inverse_mass total_inv_mass = inv_mass_a + inv_mass_b;

    if (total_inv_mass <= inverse_mass{ 0 }) {
        return;
    }

    const float ratio_a = inv_mass_a / total_inv_mass;
    const float ratio_b = inv_mass_b / total_inv_mass;

    if (res->normal.y() > 0.7f) {
        object_a->airborne = false;
    }
    if (res->normal.y() < -0.7f) {
        object_b->airborne = false;
    }

    {
        constexpr length slop = meters(0.005f);
        constexpr float baumgarte = 0.2f;
        const length corrected_pen = std::max(res->penetration - slop, length{ 0 });
        const vec3<length> correction = res->normal * corrected_pen * baumgarte;

        if (!object_a->position_locked) {
            object_a->accumulators.position_correction += correction * ratio_a;
        }
        if (!object_b->position_locked) {
            object_b->accumulators.position_correction -= correction * ratio_b;
        }
    }

    const physics::inv_inertia_mat inv_i_a = object_a->position_locked ? physics::inv_inertia_mat(0.0f) : object_a->inv_inertial_tensor();
    const physics::inv_inertia_mat inv_i_b = object_b->position_locked ? physics::inv_inertia_mat(0.0f) : object_b->inv_inertial_tensor();

    for (auto& contact_point : res->contact_points) {
        const auto r_a = contact_point - object_a->current_position;
        const auto r_b = contact_point - object_b->current_position;

        const auto vel_a = object_a->current_velocity + cross(object_a->angular_velocity, r_a);
        const auto vel_b = object_b->current_velocity + cross(object_b->angular_velocity, r_b);
        const auto relative_velocity = vel_a - vel_b;

        const velocity vel_along_normal = dot(relative_velocity, res->normal);

        if (vel_along_normal > velocity{ 0 }) {
            continue;
        }

        const auto tangent_velocity = relative_velocity - vel_along_normal * res->normal;
        const auto tangent_speed = magnitude(tangent_velocity).as<meters_per_second>();
        const auto normal_speed = abs(vel_along_normal).as<meters_per_second>();

        if (constexpr float wake_threshold = 0.2f; normal_speed > wake_threshold || tangent_speed > wake_threshold) {
            object_a->sleeping = false;
            object_a->sleep_time = 0.f;
            object_b->sleeping = false;
            object_b->sleep_time = 0.f;
        }

        float restitution = 0.0f;
        if (!object_a->self_controlled && !object_b->self_controlled) {
            if (normal_speed >= 2.0f) {
                restitution = 0.3f;
            } else if (normal_speed > 0.5f) {
                restitution = 0.3f * (normal_speed - 0.5f) / 1.5f;
            }
        }

        const auto rcross_a_part = cross(inv_i_a * cross(r_a, res->normal), r_a);
        const auto rcross_b_part = cross(inv_i_b * cross(r_b, res->normal), r_b);

        const inverse_mass denom = total_inv_mass + dot(rcross_a_part, res->normal) + dot(rcross_b_part, res->normal);
        const auto jn = -(1.0f + restitution) * vel_along_normal / denom;

        const auto impulse_vec = res->normal * jn;

        if (magnitude(tangent_velocity) > meters_per_second(1e-4f)) {
            const auto t = normalize(tangent_velocity);

            const auto rcross_a_t = cross(inv_i_a * cross(r_a, t), r_a);
            const auto rcross_b_t = cross(inv_i_b * cross(r_b, t), r_b);

            const inverse_mass denom_t =
                total_inv_mass
                + dot(rcross_a_t, t)
                + dot(rcross_b_t, t);

            auto jt = -dot(relative_velocity, t) / denom_t;

            constexpr float mu = 0.6f;
            jt = std::clamp(jt, -jn * mu, jn * mu);
            const auto friction_impulse = t * jt;

            object_a->current_velocity += friction_impulse * inv_mass_a;
            object_a->angular_velocity += inv_i_a * cross(r_a, friction_impulse);

            object_b->current_velocity -= friction_impulse * inv_mass_b;
            object_b->angular_velocity -= inv_i_b * cross(r_b, friction_impulse);
        }

        if (!object_a->position_locked) {
            object_a->current_velocity += impulse_vec * inv_mass_a;
            object_a->angular_velocity += 0.4f * inv_i_a * cross(r_a, impulse_vec);
        }
        if (!object_b->position_locked) {
            object_b->current_velocity -= impulse_vec * inv_mass_b;
            object_b->angular_velocity -= 0.4f * inv_i_b * cross(r_b, impulse_vec);
        }
    }
}

auto gse::narrow_phase_collision::support_obb(const bounding_box& bb, const unitless::vec3& dir) -> vec3<length> {
    vec3<length> result = bb.center();
    const auto he = bb.half_extents();
    const auto obb = bb.obb();

    constexpr float tol = 1e-6f;

    for (int i = 0; i < 3; ++i) {
        const float d = dot(dir, obb.axes[i]);

        float s = 0.0f;
        if (d > tol) s = 1.0f;
        if (d < -tol) s = -1.0f;

        result += obb.axes[i] * he[i] * s;
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

    unitless::vec3 dir = normalize(bb1.center() - bb2.center());

    if (is_zero(dir)) {
        dir = { 1.0f, 0.0f, 0.0f };
    }

    minkowski_point v0 = minkowski_difference(bb1, bb2, dir);

    dir = normalize(-v0.point);

    minkowski_point v1 = minkowski_difference(bb1, bb2, dir);

    if (dot(v1.point, dir) <= 0.0f) {
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
        return std::nullopt;
    }

    dir = normalize(cross(v1.point - v0.point, v2.point - v0.point));

    if (dot(dir, -v0.point) < 0.0f) {
        dir = -dir;
    }

    minkowski_point v3 = minkowski_difference(bb1, bb2, dir);

    if (dot(v3.point, dir) <= 0.0f) {
        return std::nullopt;
    }

    auto face_normal_outward = [](const vec3<length>& a,
        const vec3<length>& b,
        const vec3<length>& c,
        const vec3<length>& opp) -> unitless::vec3
        {
            unitless::vec3 n = normalize(cross(b - a, c - a));
            if (is_zero(n)) return n;

            if (dot(n, opp - a) > 0) {
                n = -n;
            }
            return n;
        };

    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        unitless::vec3 n_a = face_normal_outward(v0.point, v1.point, v2.point, v3.point);
        unitless::vec3 n_b = face_normal_outward(v0.point, v2.point, v3.point, v1.point);
        unitless::vec3 n_c = face_normal_outward(v0.point, v3.point, v1.point, v2.point);
        unitless::vec3 n_d = face_normal_outward(v1.point, v2.point, v3.point, v0.point);

        const length inf = meters(std::numeric_limits<float>::infinity());
        length d_a = is_zero(n_a) ? inf : dot(n_a, v0.point);
        length d_b = is_zero(n_b) ? inf : dot(n_b, v0.point);
        length d_c = is_zero(n_c) ? inf : dot(n_c, v0.point);
        length d_d = is_zero(n_d) ? inf : dot(n_d, v1.point);

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
            if (const length projection_dist = dot(p.point, choice.n); projection_dist - choice.d < eps) {
                auto collision_normal = -choice.n;
                length penetration_depth = choice.d;
                penetration_depth = sat_penetration(bb1, bb2).second;

                const unitless::vec3 n = normalize(collision_normal);

                auto half_extent_on = [&](const bounding_box& bb) -> length {
                    const length maxp = dot(support_obb(bb, n), n);
                    const length minp = dot(support_obb(bb, -n), n);
                    return (maxp - minp) * 0.5f;
                    };

                const length max_depth = half_extent_on(bb1) + half_extent_on(bb2);
                penetration_depth = std::min(penetration_depth, max_depth);

                if (const unitless::vec3 center_dir = normalize(bb2.center() - bb1.center()); !is_zero(center_dir) && dot(collision_normal, center_dir) < 0.0f) {
                    collision_normal = -collision_normal;
                }

                std::vector<vec3<length>> contact_points = generate_contact_points(
	                bb1,
	                bb2,
	                collision_normal
                );

                return mpr_result{
                    .collided = true,
                    .normal = collision_normal,
                    .penetration = penetration_depth,
                    .contact_points = contact_points
                };
            }

            const vec3<length>& opp = vertex_point(choice.id);
            const vec3<length> delta = p.point - opp;
            const auto delta2 = dot(delta, delta);

            bool skip_face = false;
            if (delta2 <= eps2) {
                skip_face = true;
            }
            else {
                auto equals_within_eps2 = [&](const vec3<length>& q) {
                    return dot(p.point - q, p.point - q) <= eps2;
                    };

                if (equals_within_eps2(v0.point) || equals_within_eps2(v1.point) ||
                    equals_within_eps2(v2.point) || equals_within_eps2(v3.point)) {
                    skip_face = true;
                }
            }

            if (skip_face) {
                faces[choice.id].d = meters(std::numeric_limits<float>::infinity());
                choice = pick(true);
                if (choice.d == meters(std::numeric_limits<float>::infinity())) {
                    choice = pick(false);
                }
                if (is_zero(choice.n)) {
                    break;
                }
                continue;
            }

            replace_vertex(choice.id, p);
            progressed = true;
        }

        if (!progressed) {
            break;
        }
    }

    return std::nullopt;
}

auto gse::narrow_phase_collision::generate_contact_points(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& collision_normal) -> std::vector<vec3<length>> {
    struct face_info {
        std::array<vec3<length>, 4> vertices;
        unitless::vec3 normal;
        float max_dot;
    };

    auto find_best_face = [](const bounding_box& bb, const unitless::vec3& dir) -> face_info {
        float max_dot = -std::numeric_limits<float>::max();
        int best_face_idx = -1;

        const auto& normals = bb.face_normals();
        for (std::size_t i = 0; i < 6; ++i) {
            if (const float dot_product = dot(normals[i], dir); dot_product > max_dot) {
                max_dot = dot_product;
                best_face_idx = static_cast<int>(i);
            }
        }
        return face_info{ bb.face_vertices(best_face_idx), normals[best_face_idx], max_dot };
        };

    std::vector<vec3<length>> contacts;

    unitless::vec3 n = normalize(collision_normal);
    if (dot(n, (bb2.center() - bb1.center())) < 0.0f) n = -n;

    const auto info1 = find_best_face(bb1, n);
    const auto info2 = find_best_face(bb2, -n);

	constexpr float face_similarity_threshold = 0.95f;

    if (!((info1.max_dot >= face_similarity_threshold) ||
        (info2.max_dot >= face_similarity_threshold))) {
        const auto mk = minkowski_difference(bb1, bb2, n);

        const vec3<length> c = (mk.support_a + mk.support_b) * 0.5f;
        contacts.push_back(c);
        return contacts;
    }

    std::vector<vec3<length>> inc_face_poly;
    std::array<vec3<length>, 4> ref_face;
    unitless::vec3 ref_normal;

    if (info1.max_dot > info2.max_dot) {
        ref_face = info1.vertices;
        inc_face_poly.assign(info2.vertices.begin(), info2.vertices.end());
        ref_normal = info1.normal;
    }
    else {
        ref_face = info2.vertices;
        inc_face_poly.assign(info1.vertices.begin(), info1.vertices.end());
        ref_normal = info2.normal;
    }

    ref_normal = normalize(ref_normal);

    struct plane {
        unitless::vec3 normal;
        length distance;
    };

    auto clip = [](const std::vector<vec3<length>>& subject, const plane& p) {
        std::vector<vec3<length>> out;
        if (subject.empty()) return out;

        auto prev = subject.back();
        length prev_dist = dot(p.normal, prev) - p.distance;
        bool prev_inside = prev_dist >= -meters(1e-4f);

        for (const auto& curr : subject) {
            length curr_dist = dot(p.normal, curr) - p.distance;
            bool curr_inside = curr_dist >= -meters(1e-4f);

            if (prev_inside != curr_inside) {
                length denom = prev_dist - curr_dist;
                if (abs(denom) > meters(1e-6f)) {
                    float t = prev_dist / denom;
                    out.push_back(prev + (curr - prev) * t);
                }
            }

            if (curr_inside) {
                out.push_back(curr);
            }

            prev = curr;
            prev_dist = curr_dist;
            prev_inside = curr_inside;
        }

        return out;
        };

    vec3<length> ref_center = { length{ 0 }, length{ 0 }, length{ 0 } };
    for (const auto& v : ref_face) {
        ref_center += v;
    }
    ref_center /= 4.0f;

    for (size_t i = 0; i < 4; ++i) {
        if (inc_face_poly.empty()) {
            break;
        }

        const vec3<length>& v1 = ref_face[i];
        const vec3<length>& v2 = ref_face[(i + 1) % 4];

        const auto edge_vec = v2 - v1;
        if (magnitude(edge_vec) < meters(1e-6f)) {
            continue;
        }
        const unitless::vec3 edge = normalize(edge_vec);
        unitless::vec3 plane_normal = cross(ref_normal, edge);
        const float len = magnitude(plane_normal);
        if (len < 1e-6f) {
            continue;
        }
        plane_normal /= len;

        vec3<length> to_center = ref_center - v1;

        if (dot(plane_normal, to_center) < 0.0f) {
            plane_normal = -plane_normal;
        }

        plane p{ plane_normal, dot(plane_normal, v1) };
        inc_face_poly = clip(inc_face_poly, p);
    }

    plane ref_plane{ ref_normal, dot(ref_normal, ref_face[0]) };

    for (const auto& v : inc_face_poly) {
        length dist = dot(ref_normal, v) - ref_plane.distance;

        if (dist <= meters(1e-3f)) {
            contacts.push_back(v - ref_normal * dist);
        }
    }

    return contacts;
}

auto gse::narrow_phase_collision::sat_penetration(const bounding_box& bb1, const bounding_box& bb2)
-> std::pair<unitless::vec3, length>
{
    length min_pen = meters(std::numeric_limits<float>::max());
    unitless::vec3 best_axis;

    auto test_axis = [&](unitless::vec3 axis) {
        if (is_zero(axis)) return;
        axis = normalize(axis);

        auto project = [&](const bounding_box& bb) {
            length r = 0;
			const auto he = bb.half_extents();
            for (int i = 0; i < 3; ++i) {
                r += abs(dot(axis, bb.obb().axes[i]) * he[i]);
            }
            return r;
            };

        length r1 = project(bb1);
        length r2 = project(bb2);
        length dist = abs(dot(axis, bb1.center() - bb2.center()));
        length overlap = r1 + r2 - dist;

        if (overlap > 0 && overlap < min_pen) {
            min_pen = overlap;
            best_axis = axis;
        }
        };

    for (const auto& axis : bb1.obb().axes) test_axis(axis);
    for (const auto& axis : bb2.obb().axes) test_axis(axis);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            test_axis(cross(bb1.obb().axes[i], bb2.obb().axes[j]));
        }
    }

    return { best_axis, min_pen };
}

auto gse::narrow_phase_collision::sat_speculative(
    const bounding_box& bb1,
    const bounding_box& bb2,
    const length speculative_margin
) -> std::optional<sat_result> {
    length min_overlap = meters(std::numeric_limits<float>::max());
    unitless::vec3 best_axis;
    bool all_positive = true;

    auto test_axis = [&](unitless::vec3 axis) {
        if (is_zero(axis)) return true;
        axis = normalize(axis);

        auto project = [&](const bounding_box& bb) {
            length r = meters(0.f);
            const auto he = bb.half_extents();
            for (int i = 0; i < 3; ++i) {
                r += abs(dot(axis, bb.obb().axes[i]) * he[i]);
            }
            return r;
        };

        const length r1 = project(bb1);
        const length r2 = project(bb2);
        const length dist = abs(dot(axis, bb1.center() - bb2.center()));
        const length overlap = r1 + r2 - dist;

        if (overlap < -speculative_margin) {
            return false;
        }

        if (overlap <= meters(0.f)) {
            all_positive = false;
        }

        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = axis;
        }
        return true;
    };

    for (int i = 0; i < 3; ++i) {
        if (!test_axis(bb1.obb().axes[i])) return std::nullopt;
        if (!test_axis(bb2.obb().axes[i])) return std::nullopt;
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!test_axis(cross(bb1.obb().axes[i], bb2.obb().axes[j]))) return std::nullopt;
        }
    }

    if (dot(best_axis, bb2.center() - bb1.center()) < 0.f) {
        best_axis = -best_axis;
    }

    return sat_result{
        .normal = best_axis,
        .separation = min_overlap,
        .is_speculative = !all_positive
    };
}

auto gse::narrow_phase_collision::generate_manifold(
    const bounding_box& bb1,
    const bounding_box& bb2,
    const unitless::vec3& normal,
    const length separation
) -> contact_manifold {
    contact_manifold manifold;

    auto [tangent_u, tangent_v] = compute_tangent_basis(normal);
    manifold.tangent_u = tangent_u;
    manifold.tangent_v = tangent_v;

    const auto derive_feature = [&](std::uint8_t point_ordinal) -> feature_id {
        int best = 0;
        float best_d = 0.f;
        for (int i = 0; i < 3; ++i) {
            const float d = std::abs(dot(normal, bb1.obb().axes[i]));
            if (d > best_d) { best_d = d; best = i; }
        }
        return {
            .type_a = feature_type::face,
            .type_b = feature_type::face,
            .index_a = static_cast<std::uint8_t>(best),
            .index_b = point_ordinal
        };
    };

    auto contact_points = generate_contact_points(bb1, bb2, normal);

    std::uint8_t ordinal = 0;
    for (const auto& cp : contact_points) {
        if (manifold.point_count >= 4) break;

        manifold.add_point(contact_point{
            .position_on_a = cp,
            .position_on_b = cp,
            .normal = normal,
            .separation = -separation,
            .feature = derive_feature(ordinal)
        });
        ++ordinal;
    }

    if (manifold.point_count == 0) {
        const auto mk = minkowski_difference(bb1, bb2, normal);

        manifold.add_point(contact_point{
            .position_on_a = mk.support_a,
            .position_on_b = mk.support_b,
            .normal = normal,
            .separation = -separation,
            .feature = derive_feature(0)
        });
    }

    return manifold;
}
