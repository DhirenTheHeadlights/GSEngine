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
	constexpr float sat_axis_relative_tolerance = 0.95f;
	constexpr float sat_axis_absolute_tolerance = 0.01f;

	enum class sat_axis_source { face, cross };

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

	struct face_info {
		std::uint8_t face_index = 0;
		std::array<vec3<length>, 4> vertices;
		unitless::vec3 normal;
		float max_dot = -std::numeric_limits<float>::max();
	};

	struct active_sides {
		std::array<std::uint8_t, 2> values = { feature_side_none, feature_side_none };
		std::uint8_t count = 0;
	};

	struct clip_vertex {
		vec3<length> point;
		active_sides reference_sides;
		active_sides incident_sides;
	};

	struct clipped_face_contacts {
		std::vector<clip_vertex> vertices;
		bool reference_is_a = true;
		std::uint8_t reference_face = 0;
		std::uint8_t incident_face = 0;
	};

	struct plane {
		unitless::vec3 normal;
		length distance;
	};

	struct sat_axis_choice {
		unitless::vec3 axis = {};
		length overlap = meters(std::numeric_limits<float>::max());
		float extent_scale = 0.f;
		sat_axis_source source = sat_axis_source::face;
		bool valid = false;
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

	auto add_side(
		active_sides& sides,
		std::uint8_t side
	) -> void;

	auto shared_sides(
		const active_sides& lhs,
		const active_sides& rhs
	) -> active_sides;

	auto vertex_side_set(
		std::uint8_t ordinal
	) -> active_sides;

	auto feature_type_from_sides(
		const active_sides& sides
	) -> feature_type;

	auto find_best_face_info(
		const bounding_box& bb,
		const unitless::vec3& dir
	) -> face_info;

	auto reference_edge_side_id(
		std::size_t edge_index
	) -> std::uint8_t;

	auto clip_polygon(
		const std::vector<clip_vertex>& subject,
		const plane& p,
		bool keep_greater,
		bool tag_reference_side,
		std::uint8_t reference_side
	) -> std::vector<clip_vertex>;

	auto build_clipped_face_contacts(
		const bounding_box& bb1,
		const bounding_box& bb2,
		const unitless::vec3& collision_normal
	) -> std::optional<clipped_face_contacts>;

	auto build_feature_from_clip_vertex(
		const clipped_face_contacts& clipped,
		const clip_vertex& vertex
	) -> feature_id;

	auto generate_contact_points(
		const bounding_box& bb1,
		const bounding_box& bb2,
		const unitless::vec3& collision_normal
	) -> std::vector<vec3<length>>;

	auto mpr_collision(
		const bounding_box& bb1,
		const bounding_box& bb2
	) -> std::optional<mpr_result>;

	auto should_replace_sat_choice(
		const sat_axis_choice& best,
		length overlap,
		float extent_scale
	) -> bool;

	auto update_sat_choice(
		sat_axis_choice& best,
		const unitless::vec3& axis,
		length overlap,
		sat_axis_source source,
		float extent_scale
	) -> void;

	auto prefer_face_sat_axis(
		sat_axis_choice best,
		const sat_axis_choice& best_face
	) -> sat_axis_choice;

	auto sat_penetration(
		const bounding_box& bb1,
		const bounding_box& bb2
	) -> std::pair<unitless::vec3, length>;
}

auto gse::narrow_phase_collision::support_obb(const bounding_box& bb, const unitless::vec3& dir) -> vec3<length> {
	vec3<length> result = bb.center();
	const auto he = bb.half_extents();
	const auto obb = bb.obb();

	for (int i = 0; i < 3; ++i) {
		constexpr float tol = 1e-6f;
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

auto gse::narrow_phase_collision::add_side(active_sides& sides, const std::uint8_t side) -> void {
	if (side == feature_side_none) {
		return;
	}

	for (std::uint8_t i = 0; i < sides.count; ++i) {
		if (sides.values[i] == side) {
			return;
		}
	}

	if (sides.count >= sides.values.size()) {
		return;
	}

	sides.values[sides.count++] = side;
	std::sort(sides.values.begin(), sides.values.begin() + sides.count);
}

auto gse::narrow_phase_collision::shared_sides(const active_sides& lhs, const active_sides& rhs) -> active_sides {
	active_sides shared;
	for (std::uint8_t i = 0; i < lhs.count; ++i) {
		for (std::uint8_t j = 0; j < rhs.count; ++j) {
			if (lhs.values[i] == rhs.values[j]) {
				add_side(shared, lhs.values[i]);
			}
		}
	}
	return shared;
}

auto gse::narrow_phase_collision::vertex_side_set(const std::uint8_t ordinal) -> active_sides {
	active_sides sides;
	switch (ordinal % 4) {
	case 0:
		add_side(sides, 0);
		add_side(sides, 2);
		break;
	case 1:
		add_side(sides, 1);
		add_side(sides, 2);
		break;
	case 2:
		add_side(sides, 1);
		add_side(sides, 3);
		break;
	default:
		add_side(sides, 0);
		add_side(sides, 3);
		break;
	}
	return sides;
}

auto gse::narrow_phase_collision::feature_type_from_sides(const active_sides& sides) -> feature_type {
	return sides.count == 0
		? feature_type::face
		: (sides.count == 1 ? feature_type::edge : feature_type::vertex);
}

auto gse::narrow_phase_collision::find_best_face_info(const bounding_box& bb, const unitless::vec3& dir) -> face_info {
	face_info best;
	const auto& normals = bb.face_normals();
	for (std::uint8_t i = 0; i < normals.size(); ++i) {
		if (const float score = dot(normals[i], dir); score > best.max_dot) {
			best.face_index = i;
			best.vertices = bb.face_vertices(i);
			best.normal = normals[i];
			best.max_dot = score;
		}
	}
	return best;
}

auto gse::narrow_phase_collision::reference_edge_side_id(const std::size_t edge_index) -> std::uint8_t {
	static constexpr std::array<std::uint8_t, 4> side_ids = { 2, 1, 3, 0 };
	return side_ids[edge_index % side_ids.size()];
}

auto gse::narrow_phase_collision::clip_polygon(const std::vector<clip_vertex>& subject, const plane& p, const bool keep_greater, const bool tag_reference_side, const std::uint8_t reference_side) -> std::vector<clip_vertex> {
	std::vector<clip_vertex> out;
	if (subject.empty()) {
		return out;
	}

	constexpr length clip_epsilon = meters(1e-4f);
	const auto inside = [&](const length signed_distance) {
		return keep_greater ? signed_distance >= -clip_epsilon : signed_distance <= clip_epsilon;
	};

	auto prev = subject.back();
	length prev_dist = dot(p.normal, prev.point) - p.distance;
	bool prev_inside = inside(prev_dist);

	for (auto curr : subject) {
		const length curr_dist = dot(p.normal, curr.point) - p.distance;
		const bool curr_inside = inside(curr_dist);

		if (prev_inside != curr_inside) {
			const length denom = prev_dist - curr_dist;
			if (abs(denom) > meters(1e-6f)) {
				const float t = prev_dist / denom;
				clip_vertex intersection{
					.point = prev.point + (curr.point - prev.point) * t,
					.reference_sides = shared_sides(prev.reference_sides, curr.reference_sides),
					.incident_sides = shared_sides(prev.incident_sides, curr.incident_sides)
				};
				if (tag_reference_side) {
					add_side(intersection.reference_sides, reference_side);
				}
				out.push_back(intersection);
			}
		}

		if (curr_inside) {
			if (tag_reference_side && abs(curr_dist) <= clip_epsilon) {
				add_side(curr.reference_sides, reference_side);
			}
			out.push_back(curr);
		}

		prev = curr;
		prev_dist = curr_dist;
		prev_inside = curr_inside;
	}

	return out;
}

auto gse::narrow_phase_collision::build_clipped_face_contacts(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& collision_normal) -> std::optional<clipped_face_contacts> {
	unitless::vec3 n = normalize(collision_normal);
	if (dot(n, bb2.center() - bb1.center()) < length{}) {
		n = -n;
	}

	const auto info1 = find_best_face_info(bb1, n);
	const auto info2 = find_best_face_info(bb2, -n);

	if (constexpr float reference_face_similarity_threshold = 0.85f; std::max(info1.max_dot, info2.max_dot) < reference_face_similarity_threshold) {
		return std::nullopt;
	}

	constexpr float reference_face_tie_break = 1e-4f;
	const bool reference_is_a = info1.max_dot >= info2.max_dot - reference_face_tie_break;
	const auto& reference_info = reference_is_a ? info1 : info2;
	const auto& incident_info = reference_is_a ? info2 : info1;

	std::vector<clip_vertex> polygon;
	polygon.reserve(incident_info.vertices.size());
	for (std::uint8_t i = 0; i < incident_info.vertices.size(); ++i) {
		polygon.push_back(clip_vertex{
			.point = incident_info.vertices[i],
			.incident_sides = vertex_side_set(i)
		});
	}

	const auto reference_normal = normalize(reference_info.normal);
	vec3<length> reference_center = {};
	for (const auto& v : reference_info.vertices) {
		reference_center += v;
	}
	reference_center /= 4.0f;

	for (std::size_t i = 0; i < reference_info.vertices.size(); ++i) {
		if (polygon.empty()) {
			break;
		}

		const auto& v1 = reference_info.vertices[i];
		const auto& v2 = reference_info.vertices[(i + 1) % reference_info.vertices.size()];

		const auto edge_vec = v2 - v1;
		if (magnitude(edge_vec) < meters(1e-6f)) {
			continue;
		}

		const auto edge = normalize(edge_vec);
		unitless::vec3 plane_normal = cross(reference_normal, edge);
		const float plane_length = magnitude(plane_normal);
		if (plane_length < 1e-6f) {
			continue;
		}
		plane_normal /= plane_length;

		if (dot(plane_normal, reference_center - v1) < meters(0.0f)) {
			plane_normal = -plane_normal;
		}

		polygon = clip_polygon(
			polygon,
			plane{
				.normal = plane_normal,
				.distance = dot(plane_normal, v1)
			},
			true,
			true,
			reference_edge_side_id(i)
		);
	}

	if (polygon.empty()) {
		return clipped_face_contacts{
			.reference_is_a = reference_is_a,
			.reference_face = reference_info.face_index,
			.incident_face = incident_info.face_index
		};
	}

	const plane reference_plane{
		.normal = reference_normal,
		.distance = dot(reference_normal, reference_info.vertices[0])
	};

	polygon = clip_polygon(
		polygon,
		reference_plane,
		false,
		false,
		feature_side_none
	);

	const float face_alignment = dot(reference_normal, -normalize(incident_info.normal));
	if (constexpr float slanted_face_alignment_threshold = 0.985f; !polygon.empty() && face_alignment < slanted_face_alignment_threshold) {
		float max_incident_span = 0.f;
		for (std::size_t i = 0; i < incident_info.vertices.size(); ++i) {
			const auto edge_length = magnitude(
				incident_info.vertices[(i + 1) % incident_info.vertices.size()] - incident_info.vertices[i]
			).as<meters>();
			max_incident_span = std::max(max_incident_span, edge_length);
		}

		const length contact_band = meters(std::clamp(max_incident_span * 0.1f, 0.01f, 0.1f));
		length closest_plane_distance = meters(std::numeric_limits<float>::lowest());
		for (const auto& vertex : polygon) {
			closest_plane_distance = std::max(
				closest_plane_distance,
				dot(reference_normal, vertex.point) - reference_plane.distance
			);
		}

		std::vector<clip_vertex> filtered;
		filtered.reserve(polygon.size());
		for (const auto& vertex : polygon) {
			if (const length dist = dot(reference_normal, vertex.point) - reference_plane.distance; closest_plane_distance - dist <= contact_band) {
				filtered.push_back(vertex);
			}
		}

		if (!filtered.empty()) {
			polygon = std::move(filtered);
		}
	}

	for (auto& vertex : polygon) {
		const length dist = dot(reference_normal, vertex.point) - reference_plane.distance;
		vertex.point -= reference_normal * dist;
	}

	return clipped_face_contacts{
		.vertices = std::move(polygon),
		.reference_is_a = reference_is_a,
		.reference_face = reference_info.face_index,
		.incident_face = incident_info.face_index
	};
}

auto gse::narrow_phase_collision::build_feature_from_clip_vertex(const clipped_face_contacts& clipped, const clip_vertex& vertex) -> feature_id {
	if (clipped.reference_is_a) {
		return feature_id{
			.type_a = feature_type_from_sides(vertex.reference_sides),
			.type_b = feature_type_from_sides(vertex.incident_sides),
			.index_a = clipped.reference_face,
			.index_b = clipped.incident_face,
			.side_a0 = vertex.reference_sides.values[0],
			.side_a1 = vertex.reference_sides.values[1],
			.side_b0 = vertex.incident_sides.values[0],
			.side_b1 = vertex.incident_sides.values[1]
		};
	}

	return feature_id{
		.type_a = feature_type_from_sides(vertex.incident_sides),
		.type_b = feature_type_from_sides(vertex.reference_sides),
		.index_a = clipped.incident_face,
		.index_b = clipped.reference_face,
		.side_a0 = vertex.incident_sides.values[0],
		.side_a1 = vertex.incident_sides.values[1],
		.side_b0 = vertex.reference_sides.values[0],
		.side_b1 = vertex.reference_sides.values[1]
	};
}

auto gse::narrow_phase_collision::generate_contact_points(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& collision_normal) -> std::vector<vec3<length>> {
	if (const auto clipped = build_clipped_face_contacts(bb1, bb2, collision_normal)) {
		std::vector<vec3<length>> contacts;
		contacts.reserve(clipped->vertices.size());
		for (const auto& vertex : clipped->vertices) {
			contacts.push_back(vertex.point);
		}
		if (!contacts.empty()) {
			return contacts;
		}
	}

	std::vector<vec3<length>> contacts;
	unitless::vec3 n = normalize(collision_normal);
	if (dot(n, bb2.center() - bb1.center()) < length{}) {
		n = -n;
	}

	const auto mk = minkowski_difference(bb1, bb2, n);
	contacts.push_back((mk.support_a + mk.support_b) * 0.5f);
	return contacts;
}

auto gse::narrow_phase_collision::mpr_collision(const bounding_box& bb1, const bounding_box& bb2) -> std::optional<mpr_result> {
	constexpr length eps = meters(1e-4f);
	constexpr auto eps2 = eps * eps;

	const auto obb1 = bb1.obb();
	const auto obb2 = bb2.obb();

	unitless::vec3 dir = normalize(bb1.center() - bb2.center());

	if (is_zero(dir)) {
		dir = { 1.0f, 0.0f, 0.0f };
	}

	minkowski_point v0 = minkowski_difference(bb1, bb2, dir);

	dir = normalize(-v0.point);

	minkowski_point v1 = minkowski_difference(bb1, bb2, dir);

	if (dot(v1.point, dir) <= meters(0.0f)) {
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

	if (dot(v2.point, dir) <= meters(0.0f)) {
		return std::nullopt;
	}

	dir = normalize(cross(v1.point - v0.point, v2.point - v0.point));

	if (dot(dir, -v0.point) < meters(0.0f)) {
		dir = -dir;
	}

	minkowski_point v3 = minkowski_difference(bb1, bb2, dir);

	if (dot(v3.point, dir) <= meters(0.0f)) {
		return std::nullopt;
	}

	auto face_normal_outward = [](const vec3<length>& a, const vec3<length>& b, const vec3<length>& c, const vec3<length>& opp) -> unitless::vec3 {
		unitless::vec3 n = normalize(cross(b - a, c - a));
		if (is_zero(n)) return n;

		if (dot(n, opp - a) > meters(0.0f)) {
			n = -n;
		}
		return n;
	};

	for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
		unitless::vec3 n_a = face_normal_outward(v0.point, v1.point, v2.point, v3.point);
		unitless::vec3 n_b = face_normal_outward(v0.point, v2.point, v3.point, v1.point);
		unitless::vec3 n_c = face_normal_outward(v0.point, v3.point, v1.point, v2.point);
		unitless::vec3 n_d = face_normal_outward(v1.point, v2.point, v3.point, v0.point);

		constexpr length inf = meters(std::numeric_limits<float>::infinity());
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

		auto pick = [&](const bool require_positive) -> face {
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

		auto replace_vertex = [&](const int face_id, const minkowski_point& p) {
			switch (face_id) {
			case 0:  v3 = p; break;
			case 1:  v1 = p; break;
			case 2:  v2 = p; break;
			case 3:  v0 = p; break;
			default: v3 = p; break;
			}
		};

		auto vertex_point = [&](const int face_id) -> const vec3<length>& {
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

				std::vector<vec3<length>> contact_points = generate_contact_points(bb1, bb2, collision_normal);

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

auto gse::narrow_phase_collision::should_replace_sat_choice(const sat_axis_choice& best, const length overlap, const float extent_scale) -> bool {
	if (!best.valid) {
		return true;
	}

	if (overlap >= best.overlap) {
		return false;
	}

	const length replace_threshold =
		best.overlap * sat_axis_relative_tolerance -
		meters(sat_axis_absolute_tolerance * std::max(extent_scale, 1e-3f));

	return overlap < replace_threshold;
}

auto gse::narrow_phase_collision::update_sat_choice(sat_axis_choice& best, const unitless::vec3& axis, const length overlap, const sat_axis_source source, const float extent_scale) -> void {
	if (should_replace_sat_choice(best, overlap, extent_scale)) {
		best = sat_axis_choice{
			.axis = axis,
			.overlap = overlap,
			.extent_scale = extent_scale,
			.source = source,
			.valid = true
		};
	}
}

auto gse::narrow_phase_collision::prefer_face_sat_axis(sat_axis_choice best, const sat_axis_choice& best_face) -> sat_axis_choice {
	if (!best.valid || best.source != sat_axis_source::cross || !best_face.valid) {
		return best;
	}

	const length cross_replace_threshold =
		best_face.overlap * sat_axis_relative_tolerance -
		meters(sat_axis_absolute_tolerance * std::max(best_face.extent_scale, 1e-3f));

	if (best.overlap >= cross_replace_threshold) {
		return best_face;
	}

	return best;
}

auto gse::narrow_phase_collision::sat_penetration(const bounding_box& bb1, const bounding_box& bb2) -> std::pair<unitless::vec3, length> {
	sat_axis_choice best_axis;
	sat_axis_choice best_face_axis;

	auto test_axis = [&](unitless::vec3 axis, const sat_axis_source source, const float extent_scale) {
		if (is_zero(axis)) return;
		axis = normalize(axis);

		auto project = [&](const bounding_box& bb) {
			length r = meters(0);
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

		if (overlap > meters(0.f)) {
			update_sat_choice(best_axis, axis, overlap, source, extent_scale);
			if (source == sat_axis_source::face) {
				update_sat_choice(best_face_axis, axis, overlap, source, extent_scale);
			}
		}
	};

	auto half = bb1.half_extents();
	auto half2 = bb2.half_extents();

	for (int i = 0; i < 3; ++i) {
		test_axis(bb1.obb().axes[i], sat_axis_source::face, half[i].as<meters>());
		test_axis(bb2.obb().axes[i], sat_axis_source::face, half2[i].as<meters>());
	}
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			test_axis(cross(bb1.obb().axes[i], bb2.obb().axes[j]), sat_axis_source::cross, 0.f);
		}
	}

	best_axis = prefer_face_sat_axis(best_axis, best_face_axis);
	return { best_axis.axis, best_axis.overlap };
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_a, physics::collision_component& coll_a, physics::motion_component* object_b, physics::collision_component& coll_b) -> void {
	if (!object_a || !object_b) {
		return;
	}

	auto res = mpr_collision(coll_a.bounding_box, coll_b.bounding_box);
	if (!res) {
		return;
	}

	if (dot(object_a->current_position - object_b->current_position, res->normal) < meters(0.0f)) {
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
			object_b->sleeping = false;
		}

		float restitution = 0.0f;
		if (normal_speed >= 2.0f) {
			restitution = 0.3f;
		} else if (normal_speed > 0.5f) {
			restitution = 0.3f * (normal_speed - 0.5f) / 1.5f;
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

auto gse::narrow_phase_collision::sat_speculative(const bounding_box& bb1, const bounding_box& bb2, const length speculative_margin) -> std::optional<sat_result> {
	sat_axis_choice best_axis;
	sat_axis_choice best_face_axis;
	bool all_positive = true;

	auto test_axis = [&](unitless::vec3 axis, const sat_axis_source source, const float extent_scale) {
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

		update_sat_choice(best_axis, axis, overlap, source, extent_scale);
		if (source == sat_axis_source::face) {
			update_sat_choice(best_face_axis, axis, overlap, source, extent_scale);
		}
		return true;
	};

	auto half = bb1.half_extents();
	auto half2 = bb2.half_extents();
	for (int i = 0; i < 3; ++i) {
		if (!test_axis(bb1.obb().axes[i], sat_axis_source::face, half[i].as<meters>())) return std::nullopt;
		if (!test_axis(bb2.obb().axes[i], sat_axis_source::face, half2[i].as<meters>())) return std::nullopt;
	}

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			if (!test_axis(cross(bb1.obb().axes[i], bb2.obb().axes[j]), sat_axis_source::cross, 0.f)) return std::nullopt;
		}
	}

	best_axis = prefer_face_sat_axis(best_axis, best_face_axis);
	if (dot(best_axis.axis, bb2.center() - bb1.center()) < meters(0.0f)) {
		best_axis.axis = -best_axis.axis;
	}

	return sat_result{
		.normal = best_axis.axis,
		.separation = best_axis.overlap,
		.is_speculative = !all_positive
	};
}

auto gse::narrow_phase_collision::generate_manifold(const bounding_box& bb1, const bounding_box& bb2, const unitless::vec3& normal, const length separation) -> contact_manifold {
	contact_manifold manifold;

	auto [tangent_u, tangent_v] = compute_tangent_basis(normal);
	manifold.tangent_u = tangent_u;
	manifold.tangent_v = tangent_v;

	struct manifold_candidate {
		vec3<length> position_on_a;
		vec3<length> position_on_b;
		feature_id feature;
	};

	const length contact_gap = -separation;
	const auto candidate_midpoint = [](const manifold_candidate& candidate) {
		return (candidate.position_on_a + candidate.position_on_b) * 0.5f;
	};

	const auto quantize = [](const float value) -> int {
		return static_cast<int>(std::floor(value * 10000.f + (value >= 0.f ? 0.5f : -0.5f)));
	};

	const auto pack_feature = [](const feature_id& feature) -> std::uint64_t {
		return
			(static_cast<std::uint64_t>(static_cast<std::uint8_t>(feature.type_a)) << 56) |
			(static_cast<std::uint64_t>(feature.index_a) << 48) |
			(static_cast<std::uint64_t>(feature.side_a0) << 40) |
			(static_cast<std::uint64_t>(feature.side_a1) << 32) |
			(static_cast<std::uint64_t>(static_cast<std::uint8_t>(feature.type_b)) << 24) |
			(static_cast<std::uint64_t>(feature.index_b) << 16) |
			(static_cast<std::uint64_t>(feature.side_b0) << 8) |
			static_cast<std::uint64_t>(feature.side_b1);
	};

	const auto derive_fallback_feature = [&](const vec3<length>& point) -> feature_id {
		const auto classify = [](const bounding_box& bb, const unitless::vec3& support_dir, const vec3<length>& world_point) {
			struct detail {
				feature_type type = feature_type::face;
				std::uint8_t face = 0;
				std::uint8_t side0 = feature_side_none;
				std::uint8_t side1 = feature_side_none;
			};

			const auto normals = bb.face_normals();
			std::uint8_t face_index = 0;
			float best_dot = -std::numeric_limits<float>::max();
			for (std::uint8_t i = 0; i < normals.size(); ++i) {
				if (const float score = dot(normals[i], support_dir); score > best_dot) {
					best_dot = score;
					face_index = i;
				}
			}

			const auto half_extents = bb.half_extents();
			const auto box = bb.obb();
			const unitless::vec3 local = {
				dot(world_point - box.center, box.axes[0]).as<meters>(),
				dot(world_point - box.center, box.axes[1]).as<meters>(),
				dot(world_point - box.center, box.axes[2]).as<meters>()
			};

			const std::uint8_t axis_idx = face_index / 2;
			const std::uint8_t u_axis = (axis_idx + 1) % 3;
			const std::uint8_t v_axis = (axis_idx + 2) % 3;
			const float tangent_scale = std::max(half_extents[u_axis].as<meters>(), half_extents[v_axis].as<meters>());
			const float boundary_threshold = std::clamp(tangent_scale * 0.002f, 0.0005f, 0.01f);

			detail result{
				.face = face_index
			};

			auto boundary_distance = [&](const std::uint8_t axis) -> float {
				const float half_extent = half_extents[axis].as<meters>();
				return half_extent - std::clamp(std::abs(local[axis]), 0.f, half_extent);
			};

			if (boundary_distance(u_axis) <= boundary_threshold) {
				result.side0 = local[u_axis] >= 0.f ? 0 : 1;
			}
			if (boundary_distance(v_axis) <= boundary_threshold) {
				const std::uint8_t side = local[v_axis] >= 0.f ? 2 : 3;
				if (result.side0 == feature_side_none) {
					result.side0 = side;
				}
				else if (result.side0 != side) {
					result.side1 = side;
				}
			}

			result.type = result.side0 == feature_side_none
				? feature_type::face
				: (result.side1 == feature_side_none ? feature_type::edge : feature_type::vertex);
			if (result.side1 != feature_side_none && result.side1 < result.side0) {
				std::swap(result.side0, result.side1);
			}
			return result;
		};

		const auto detail_a = classify(bb1, normal, point);
		const auto detail_b = classify(bb2, -normal, point);
		return feature_id{
			.type_a = detail_a.type,
			.type_b = detail_b.type,
			.index_a = detail_a.face,
			.index_b = detail_b.face,
			.side_a0 = detail_a.side0,
			.side_a1 = detail_a.side1,
			.side_b0 = detail_b.side0,
			.side_b1 = detail_b.side1
		};
	};

	std::vector<manifold_candidate> candidates;
	if (const auto clipped = build_clipped_face_contacts(bb1, bb2, normal)) {
		candidates.reserve(clipped->vertices.size());
		for (const auto& vertex : clipped->vertices) {
			const vec3<length> position_on_a = clipped->reference_is_a
				? vertex.point
				: (vertex.point - normal * contact_gap);
			const vec3<length> position_on_b = clipped->reference_is_a
				? (vertex.point + normal * contact_gap)
				: vertex.point;
			candidates.push_back(manifold_candidate{
				.position_on_a = position_on_a,
				.position_on_b = position_on_b,
				.feature = build_feature_from_clip_vertex(*clipped, vertex)
			});
		}
	}

	std::ranges::sort(candidates, [&](const manifold_candidate& lhs, const manifold_candidate& rhs) {
		const std::uint64_t lhs_key = pack_feature(lhs.feature);
		const std::uint64_t rhs_key = pack_feature(rhs.feature);
		if (lhs_key != rhs_key) return lhs_key < rhs_key;

		const int lhs_u = quantize(dot(candidate_midpoint(lhs), tangent_u).as<meters>());
		const int rhs_u = quantize(dot(candidate_midpoint(rhs), tangent_u).as<meters>());
		if (lhs_u != rhs_u) return lhs_u < rhs_u;

		const int lhs_v = quantize(dot(candidate_midpoint(lhs), tangent_v).as<meters>());
		const int rhs_v = quantize(dot(candidate_midpoint(rhs), tangent_v).as<meters>());
		if (lhs_v != rhs_v) return lhs_v < rhs_v;

		return dot(candidate_midpoint(lhs), normal).as<meters>() < dot(candidate_midpoint(rhs), normal).as<meters>();
	});

	std::vector<manifold_candidate> unique_candidates;
	unique_candidates.reserve(candidates.size());
	for (const auto& candidate : candidates) {
		if (!unique_candidates.empty() && unique_candidates.back().feature == candidate.feature) {
			continue;
		}
		unique_candidates.push_back(candidate);
	}

	for (const auto& [position_on_a, position_on_b, feature] : unique_candidates) {
		if (manifold.point_count >= 4) break;

		manifold.add_point(contact_point{
			.position_on_a = position_on_a,
			.position_on_b = position_on_b,
			.normal = normal,
			.separation = meters(0.f),
			.feature = feature
		});
	}

	if (manifold.point_count == 0) {
		const auto mk = minkowski_difference(bb1, bb2, normal);
		const vec3<length> cp = (mk.support_a + mk.support_b) * 0.5f;

		manifold.add_point(contact_point{
			.position_on_a = mk.support_a,
			.position_on_b = mk.support_b,
			.normal = normal,
			.separation = meters(0.f),
			.feature = derive_fallback_feature(cp)
		});
	}

	return manifold;
}
