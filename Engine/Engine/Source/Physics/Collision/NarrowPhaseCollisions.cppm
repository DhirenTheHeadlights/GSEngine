export module gse.physics:narrow_phase_collision;

import std;

import :bounding_box;
import :collision_component;
import :contact_manifold;

import gse.math;
import gse.utility;

export namespace gse::narrow_phase_collision {
	struct sat_result {
		vec3f normal;
		length separation;
		bool is_speculative;
	};

	struct shape_data {
		const bounding_box* bb = nullptr;
		physics::shape_type type = physics::shape_type::box;
		length radius = {};
		length half_height = {};
	};

	auto sat_speculative(
		const bounding_box& bb1,
		const bounding_box& bb2,
		length speculative_margin
	) -> std::optional<sat_result>;

	auto generate_manifold(
		const bounding_box& bb1,
		const bounding_box& bb2,
		const vec3f& normal,
		length separation
	) -> contact_manifold;

	auto speculative_test(
		const shape_data& a,
		const shape_data& b,
		length speculative_margin
	) -> std::optional<sat_result>;

	auto generate_shape_manifold(
		const shape_data& a,
		const shape_data& b,
		const vec3f& normal,
		length separation
	) -> contact_manifold;
}

namespace gse::narrow_phase_collision {
	constexpr float sat_axis_relative_tolerance = 0.95f;
	constexpr float sat_axis_absolute_tolerance = 0.01f;

	enum class sat_axis_source { face, cross };

	struct minkowski_point {
		vec3<length> point;
		vec3<length> support_a;
		vec3<length> support_b;
	};

	struct face_info {
		std::uint8_t face_index = 0;
		std::array<vec3<length>, 4> vertices;
		vec3f normal;
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
		vec3f normal;
		length distance;
	};

	struct sat_axis_choice {
		vec3f axis = {};
		length overlap = meters(std::numeric_limits<float>::max());
		float extent_scale = 0.f;
		sat_axis_source source = sat_axis_source::face;
		bool valid = false;
	};

	auto support_obb(
		const bounding_box& bb,
		const vec3f& dir
	) -> vec3<length>;

	auto minkowski_difference(
		const bounding_box& bb1,
		const bounding_box& bb2,
		const vec3f& dir
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
		const vec3f& dir
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
		const vec3f& collision_normal
	) -> std::optional<clipped_face_contacts>;

	auto build_feature_from_clip_vertex(
		const clipped_face_contacts& clipped,
		const clip_vertex& vertex
	) -> feature_id;

	auto should_replace_sat_choice(
		const sat_axis_choice& best,
		length overlap,
		float extent_scale
	) -> bool;

	auto update_sat_choice(
		sat_axis_choice& best,
		const vec3f& axis,
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
	) -> std::pair<vec3f, length>;

	constexpr std::uint8_t sphere_surface_index = 6;
	constexpr std::uint8_t capsule_barrel_index = 6;
	constexpr std::uint8_t capsule_cap_a_index = 7;
	constexpr std::uint8_t capsule_cap_b_index = 8;

	struct obb_query_result {
		vec3<length> closest;
		vec3f normal;
		length signed_distance;
	};

	auto capsule_endpoints(const bounding_box& bb, length half_height) -> std::pair<vec3<length>, vec3<length>>;
	auto closest_point_on_segment(const vec3<length>& a, const vec3<length>& b, const vec3<length>& p) -> vec3<length>;
	auto segment_segment_closest_params(const vec3<length>& p1, const vec3<length>& q1, const vec3<length>& p2, const vec3<length>& q2) -> std::pair<float, float>;
	auto query_obb(const bounding_box& bb, const vec3<length>& point) -> obb_query_result;
	auto segment_obb_query(const bounding_box& bb, const vec3<length>& seg_start, const vec3<length>& seg_end) -> std::pair<vec3<length>, obb_query_result>;
	auto classify_box_face(const bounding_box& bb, const vec3f& direction) -> std::uint8_t;
	auto classify_capsule_feature(float t_param) -> std::pair<feature_type, std::uint8_t>;

	auto sphere_sphere_speculative(const vec3<length>& ca, length ra, const vec3<length>& cb, length rb, length margin) -> std::optional<sat_result>;
	auto box_sphere_speculative(const bounding_box& bb, const vec3<length>& center, length radius, length margin) -> std::optional<sat_result>;
	auto capsule_capsule_speculative(const bounding_box& bb_a, length ha, length ra, const bounding_box& bb_b, length hb, length rb, length margin) -> std::optional<sat_result>;
	auto box_capsule_speculative(const bounding_box& bb, const bounding_box& cap_bb, length half_h, length radius, length margin) -> std::optional<sat_result>;
	auto sphere_capsule_speculative(const vec3<length>& sph_center, length sph_r, const bounding_box& cap_bb, length cap_h, length cap_r, length margin) -> std::optional<sat_result>;

	auto sphere_sphere_manifold(const vec3<length>& ca, length ra, const vec3<length>& cb, length rb, const vec3f& normal, length separation) -> contact_manifold;
	auto box_sphere_manifold(const bounding_box& bb, const vec3<length>& center, length radius, const vec3f& normal, length separation) -> contact_manifold;
	auto capsule_capsule_manifold(const bounding_box& bb_a, length ha, length ra, const bounding_box& bb_b, length hb, length rb, const vec3f& normal, length separation) -> contact_manifold;
	auto box_capsule_manifold(const bounding_box& bb, const bounding_box& cap_bb, length cap_h, length cap_r, const vec3f& normal, length separation) -> contact_manifold;
	auto sphere_capsule_manifold(const vec3<length>& sph_center, length sph_r, const bounding_box& cap_bb, length cap_h, length cap_r, const vec3f& normal, length separation) -> contact_manifold;
}

auto gse::narrow_phase_collision::support_obb(const bounding_box& bb, const vec3f& dir) -> vec3<length> {
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

auto gse::narrow_phase_collision::minkowski_difference(const bounding_box& bb1, const bounding_box& bb2, const vec3f& dir) -> minkowski_point {
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

auto gse::narrow_phase_collision::find_best_face_info(const bounding_box& bb, const vec3f& dir) -> face_info {
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

auto gse::narrow_phase_collision::build_clipped_face_contacts(const bounding_box& bb1, const bounding_box& bb2, const vec3f& collision_normal) -> std::optional<clipped_face_contacts> {
	vec3f n = normalize(collision_normal);
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
		vec3f plane_normal = cross(reference_normal, edge);
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

auto gse::narrow_phase_collision::update_sat_choice(sat_axis_choice& best, const vec3f& axis, const length overlap, const sat_axis_source source, const float extent_scale) -> void {
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

auto gse::narrow_phase_collision::sat_penetration(const bounding_box& bb1, const bounding_box& bb2) -> std::pair<vec3f, length> {
	sat_axis_choice best_axis;
	sat_axis_choice best_face_axis;

	auto test_axis = [&](vec3f axis, const sat_axis_source source, const float extent_scale) {
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

auto gse::narrow_phase_collision::sat_speculative(const bounding_box& bb1, const bounding_box& bb2, const length speculative_margin) -> std::optional<sat_result> {
	sat_axis_choice best_axis;
	sat_axis_choice best_face_axis;
	bool all_positive = true;

	auto test_axis = [&](vec3f axis, const sat_axis_source source, const float extent_scale) {
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

auto gse::narrow_phase_collision::generate_manifold(const bounding_box& bb1, const bounding_box& bb2, const vec3f& normal, const length separation) -> contact_manifold {
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
		const auto classify = [](const bounding_box& bb, const vec3f& support_dir, const vec3<length>& world_point) {
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
			const vec3f local = {
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

auto gse::narrow_phase_collision::capsule_endpoints(const bounding_box& bb, const length half_height) -> std::pair<vec3<length>, vec3<length>> {
	const auto axis = mat3_cast(bb.obb().orientation) * vec3f(0.f, 1.f, 0.f);
	return { bb.center() + axis * half_height, bb.center() - axis * half_height };
}

auto gse::narrow_phase_collision::closest_point_on_segment(const vec3<length>& a, const vec3<length>& b, const vec3<length>& p) -> vec3<length> {
	const auto ab = b - a;
	if (magnitude(ab) < meters(1e-8f)) return a;
	const auto ap = p - a;
	const float t = std::clamp(dot(ap, ab) / dot(ab, ab), 0.f, 1.f);
	return a + ab * t;
}

auto gse::narrow_phase_collision::segment_segment_closest_params(
	const vec3<length>& p1, const vec3<length>& q1,
	const vec3<length>& p2, const vec3<length>& q2
) -> std::pair<float, float> {
	const auto d1 = q1 - p1;
	const auto d2 = q2 - p2;
	const auto r = p1 - p2;

	const auto a = dot(d1, d1);
	const auto e = dot(d2, d2);
	const auto f = dot(d2, r);

	constexpr auto eps = meters(1e-4f) * meters(1e-4f);

	if (a <= eps && e <= eps) {
		return { 0.f, 0.f };
	}

	float s, t;
	if (a <= eps) {
		s = 0.f;
		t = std::clamp(f / e, 0.f, 1.f);
	} else {
		const auto c = dot(d1, r);
		if (e <= eps) {
			t = 0.f;
			s = std::clamp(-c / a, 0.f, 1.f);
		} else {
			const auto b_val = dot(d1, d2);
			const auto denom = a * e - b_val * b_val;
			s = (denom > eps * eps) ? std::clamp((b_val * f - c * e) / denom, 0.f, 1.f) : 0.f;
			t = (b_val * s + f) / e;
			if (t < 0.f) {
				t = 0.f;
				s = std::clamp(-c / a, 0.f, 1.f);
			} else if (t > 1.f) {
				t = 1.f;
				s = std::clamp((b_val - c) / a, 0.f, 1.f);
			}
		}
	}

	return { s, t };
}

auto gse::narrow_phase_collision::query_obb(const bounding_box& bb, const vec3<length>& point) -> obb_query_result {
	const auto o = bb.obb();
	const auto he = bb.half_extents();
	const auto diff = point - o.center;

	std::array<length, 3> local;
	bool inside = true;
	for (int i = 0; i < 3; ++i) {
		local[i] = dot(o.axes[i], diff);
		if (abs(local[i]) > he[i]) inside = false;
	}

	if (!inside) {
		auto cp = o.center;
		for (int i = 0; i < 3; ++i) {
			cp += o.axes[i] * std::clamp(local[i], -he[i], he[i]);
		}
		const auto to_point = point - cp;
		const auto dist = magnitude(to_point);
		const vec3f normal = (dist > meters(1e-6f)) ? normalize(to_point) : o.axes[0];
		return { cp, normal, dist };
	}

	int min_axis = 0;
	length min_depth = he[0] - abs(local[0]);
	for (int i = 1; i < 3; ++i) {
		if (length depth = he[i] - abs(local[i]); depth < min_depth) {
			min_depth = depth;
			min_axis = i;
		}
	}

	const float sign = local[min_axis] >= length{} ? 1.f : -1.f;
	auto cp = o.center;
	for (int i = 0; i < 3; ++i) {
		cp += o.axes[i] * (i == min_axis ? he[i] * sign : local[i]);
	}
	return { cp, o.axes[min_axis] * sign, -min_depth };
}

auto gse::narrow_phase_collision::segment_obb_query(
	const bounding_box& bb,
	const vec3<length>& seg_start,
	const vec3<length>& seg_end
) -> std::pair<vec3<length>, obb_query_result> {
	auto seg_pt = closest_point_on_segment(seg_start, seg_end, bb.center());
	auto best = query_obb(bb, seg_pt);
	auto best_pt = seg_pt;

	for (int iter = 0; iter < 3; ++iter) {
		seg_pt = closest_point_on_segment(seg_start, seg_end, best.closest);
		if (auto result = query_obb(bb, seg_pt); result.signed_distance < best.signed_distance) {
			best = result;
			best_pt = seg_pt;
		}
	}

	for (const auto& pt : { seg_start, seg_end }) {
		if (auto result = query_obb(bb, pt); result.signed_distance < best.signed_distance) {
			best = result;
			best_pt = pt;
		}
	}

	return { best_pt, best };
}

auto gse::narrow_phase_collision::classify_box_face(const bounding_box& bb, const vec3f& direction) -> std::uint8_t {
	const auto normals = bb.face_normals();
	std::uint8_t best = 0;
	float best_dot = -std::numeric_limits<float>::max();
	for (std::uint8_t i = 0; i < 6; ++i) {
		if (const float d = dot(normals[i], direction); d > best_dot) {
			best_dot = d;
			best = i;
		}
	}
	return best;
}

auto gse::narrow_phase_collision::classify_capsule_feature(const float t) -> std::pair<feature_type, std::uint8_t> {
	constexpr float cap_threshold = 0.01f;
	if (t < cap_threshold) return { feature_type::vertex, capsule_cap_a_index };
	if (t > 1.f - cap_threshold) return { feature_type::vertex, capsule_cap_b_index };
	return { feature_type::edge, capsule_barrel_index };
}

auto gse::narrow_phase_collision::sphere_sphere_speculative(
	const vec3<length>& ca, const length ra,
	const vec3<length>& cb, const length rb,
	const length margin
) -> std::optional<sat_result> {
	const auto diff = cb - ca;
	const auto dist = magnitude(diff);
	const length separation = ra + rb - dist;

	if (separation < -margin) return std::nullopt;

	const vec3f normal = (dist > meters(1e-6f)) ? normalize(diff) : vec3f(0.f, 1.f, 0.f);

	return sat_result{
		.normal = normal,
		.separation = separation,
		.is_speculative = separation < length{}
	};
}

auto gse::narrow_phase_collision::box_sphere_speculative(
	const bounding_box& bb,
	const vec3<length>& center, const length radius,
	const length margin
) -> std::optional<sat_result> {
	const auto [closest, normal, signed_dist] = query_obb(bb, center);
	const length separation = radius - signed_dist;

	if (separation < -margin) return std::nullopt;

	return sat_result{
		.normal = normal,
		.separation = separation,
		.is_speculative = separation < length{}
	};
}

auto gse::narrow_phase_collision::capsule_capsule_speculative(
	const bounding_box& bb_a, const length ha, const length ra,
	const bounding_box& bb_b, const length hb, const length rb,
	const length margin
) -> std::optional<sat_result> {
	const auto [a0, a1] = capsule_endpoints(bb_a, ha);
	const auto [b0, b1] = capsule_endpoints(bb_b, hb);

	auto [s, t] = segment_segment_closest_params(a0, a1, b0, b1);
	const auto closest_a = a0 + (a1 - a0) * s;
	const auto closest_b = b0 + (b1 - b0) * t;

	const auto diff = closest_b - closest_a;
	const auto dist = magnitude(diff);
	const length separation = ra + rb - dist;

	if (separation < -margin) return std::nullopt;

	const vec3f normal = (dist > meters(1e-6f)) ? normalize(diff) : vec3f(0.f, 1.f, 0.f);

	return sat_result{
		.normal = normal,
		.separation = separation,
		.is_speculative = separation < length{}
	};
}

auto gse::narrow_phase_collision::box_capsule_speculative(
	const bounding_box& bb,
	const bounding_box& cap_bb, const length half_h, const length radius,
	const length margin
) -> std::optional<sat_result> {
	const auto [seg_start, seg_end] = capsule_endpoints(cap_bb, half_h);
	const auto [seg_pt, obb_result] = segment_obb_query(bb, seg_start, seg_end);

	const length separation = radius - obb_result.signed_distance;

	if (separation < -margin) return std::nullopt;

	return sat_result{
		.normal = obb_result.normal,
		.separation = separation,
		.is_speculative = separation < length{}
	};
}

auto gse::narrow_phase_collision::sphere_capsule_speculative(
	const vec3<length>& sph_center, const length sph_r,
	const bounding_box& cap_bb, const length cap_h, const length cap_r,
	const length margin
) -> std::optional<sat_result> {
	const auto [seg_start, seg_end] = capsule_endpoints(cap_bb, cap_h);
	const auto closest_on_seg = closest_point_on_segment(seg_start, seg_end, sph_center);

	const auto diff = closest_on_seg - sph_center;
	const auto dist = magnitude(diff);
	const length separation = sph_r + cap_r - dist;

	if (separation < -margin) return std::nullopt;

	const vec3f normal = (dist > meters(1e-6f)) ? normalize(diff) : vec3f(0.f, 1.f, 0.f);

	return sat_result{
		.normal = normal,
		.separation = separation,
		.is_speculative = separation < length{}
	};
}

auto gse::narrow_phase_collision::sphere_sphere_manifold(
	const vec3<length>& ca, const length ra,
	const vec3<length>& cb, const length rb,
	const vec3f& normal, const length separation
) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	manifold.add_point(contact_point{
		.position_on_a = ca + normal * ra,
		.position_on_b = cb - normal * rb,
		.normal = normal,
		.separation = meters(0.f),
		.feature = feature_id{
			.type_a = feature_type::face,
			.type_b = feature_type::face,
			.index_a = sphere_surface_index,
			.index_b = sphere_surface_index,
		}
	});

	return manifold;
}

auto gse::narrow_phase_collision::box_sphere_manifold(
	const bounding_box& bb,
	const vec3<length>& center, const length radius,
	const vec3f& normal, const length separation
) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	const auto [closest, obb_normal, signed_dist] = query_obb(bb, center);
	const auto box_face = classify_box_face(bb, normal);

	if (const auto face_normals = bb.face_normals(); dot(normal, face_normals[box_face]) > 0.9f) {
		const length ring_radius = radius * 0.3f;
		constexpr int ring_count = 4;

		for (int i = 0; i < ring_count; ++i) {
			const float angle = static_cast<float>(i) * (2.f * std::numbers::pi_v<float> / ring_count);
			const auto offset = tu * (ring_radius * std::cos(angle)) + tv * (ring_radius * std::sin(angle));

			const auto box_point = closest + offset;
			const auto sphere_point = center + normalize(box_point - center) * radius;

			manifold.add_point(contact_point{
				.position_on_a = box_point,
				.position_on_b = sphere_point,
				.normal = normal,
				.separation = meters(0.f),
				.feature = feature_id{
					.type_a = feature_type::face,
					.type_b = feature_type::face,
					.index_a = box_face,
					.index_b = sphere_surface_index,
					.side_b0 = static_cast<std::uint8_t>(i),
				}
			});
		}
	} else {
		manifold.add_point(contact_point{
			.position_on_a = closest,
			.position_on_b = center - normal * radius,
			.normal = normal,
			.separation = meters(0.f),
			.feature = feature_id{
				.type_a = feature_type::face,
				.type_b = feature_type::face,
				.index_a = box_face,
				.index_b = sphere_surface_index,
			}
		});
	}

	return manifold;
}

auto gse::narrow_phase_collision::capsule_capsule_manifold(
	const bounding_box& bb_a, const length ha, const length ra,
	const bounding_box& bb_b, const length hb, const length rb,
	const vec3f& normal, const length separation
) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	const auto [a0, a1] = capsule_endpoints(bb_a, ha);
	const auto [b0, b1] = capsule_endpoints(bb_b, hb);

	auto [s, t] = segment_segment_closest_params(a0, a1, b0, b1);
	const auto closest_a = a0 + (a1 - a0) * s;
	const auto closest_b = b0 + (b1 - b0) * t;

	auto [type_a, index_a] = classify_capsule_feature(s);
	auto [type_b, index_b] = classify_capsule_feature(t);

	manifold.add_point(contact_point{
		.position_on_a = closest_a + normal * ra,
		.position_on_b = closest_b - normal * rb,
		.normal = normal,
		.separation = meters(0.f),
		.feature = feature_id{
			.type_a = type_a,
			.type_b = type_b,
			.index_a = index_a,
			.index_b = index_b,
		}
	});

	return manifold;
}

auto gse::narrow_phase_collision::box_capsule_manifold(
	const bounding_box& bb,
	const bounding_box& cap_bb, const length cap_h, const length cap_r,
	const vec3f& normal, const length separation
) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	const auto [seg_start, seg_end] = capsule_endpoints(cap_bb, cap_h);
	const auto [seg_pt, obb_result] = segment_obb_query(bb, seg_start, seg_end);

	float t_param = 0.5f;
	if (const auto seg_diff = seg_end - seg_start; magnitude(seg_diff) > meters(1e-6f)) {
		const auto p = seg_pt - seg_start;
		t_param = std::clamp(dot(p, seg_diff) / dot(seg_diff, seg_diff), 0.f, 1.f);
	}

	auto [cap_type, cap_index] = classify_capsule_feature(t_param);

	manifold.add_point(contact_point{
		.position_on_a = obb_result.closest,
		.position_on_b = seg_pt - normal * cap_r,
		.normal = normal,
		.separation = meters(0.f),
		.feature = feature_id{
			.type_a = feature_type::face,
			.type_b = cap_type,
			.index_a = classify_box_face(bb, normal),
			.index_b = cap_index,
		}
	});

	return manifold;
}

auto gse::narrow_phase_collision::sphere_capsule_manifold(
	const vec3<length>& sph_center, const length sph_r,
	const bounding_box& cap_bb, const length cap_h, const length cap_r,
	const vec3f& normal, const length separation
) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	const auto [seg_start, seg_end] = capsule_endpoints(cap_bb, cap_h);
	const auto closest_on_seg = closest_point_on_segment(seg_start, seg_end, sph_center);

	float t_param = 0.5f;
	if (const auto seg_diff = seg_end - seg_start; magnitude(seg_diff) > meters(1e-6f)) {
		const auto p = closest_on_seg - seg_start;
		t_param = std::clamp(dot(p, seg_diff) / dot(seg_diff, seg_diff), 0.f, 1.f);
	}

	auto [cap_type, cap_index] = classify_capsule_feature(t_param);

	manifold.add_point(contact_point{
		.position_on_a = sph_center + normal * sph_r,
		.position_on_b = closest_on_seg - normal * cap_r,
		.normal = normal,
		.separation = meters(0.f),
		.feature = feature_id{
			.type_a = feature_type::face,
			.type_b = cap_type,
			.index_a = sphere_surface_index,
			.index_b = cap_index,
		}
	});

	return manifold;
}

auto gse::narrow_phase_collision::speculative_test(
	const shape_data& a, const shape_data& b, const length margin
) -> std::optional<sat_result> {
	using st = physics::shape_type;
	const bool swap = a.type > b.type;
	const auto& [lo_bb, lo_type, lo_radius, lo_half_height] = swap ? b : a;
	const auto& [hi_bb, hi_type, hi_radius, hi_half_height] = swap ? a : b;

	std::optional<sat_result> result;

	if (lo_type == st::box && hi_type == st::box) {
		result = sat_speculative(*lo_bb, *hi_bb, margin);
	} else if (lo_type == st::box && hi_type == st::sphere) {
		result = box_sphere_speculative(*lo_bb, hi_bb->center(), hi_radius, margin);
	} else if (lo_type == st::box && hi_type == st::capsule) {
		result = box_capsule_speculative(*lo_bb, *hi_bb, hi_half_height, hi_radius, margin);
	} else if (lo_type == st::sphere && hi_type == st::sphere) {
		result = sphere_sphere_speculative(lo_bb->center(), lo_radius, hi_bb->center(), hi_radius, margin);
	} else if (lo_type == st::sphere && hi_type == st::capsule) {
		result = sphere_capsule_speculative(lo_bb->center(), lo_radius, *hi_bb, hi_half_height, hi_radius, margin);
	} else if (lo_type == st::capsule && hi_type == st::capsule) {
		result = capsule_capsule_speculative(*lo_bb, lo_half_height, lo_radius, *hi_bb, hi_half_height, hi_radius, margin);
	}

	if (swap && result) {
		result->normal = -result->normal;
	}

	return result;
}

auto gse::narrow_phase_collision::generate_shape_manifold(
	const shape_data& a, const shape_data& b,
	const vec3f& normal, const length separation
) -> contact_manifold {
	using st = physics::shape_type;
	const bool swap = a.type > b.type;
	const auto& [lo_bb, lo_type, lo_radius, lo_half_height] = swap ? b : a;
	const auto& [hi_bb, hi_type, hi_radius, hi_half_height] = swap ? a : b;
	const auto n = swap ? -normal : normal;

	contact_manifold manifold;

	if (lo_type == st::box && hi_type == st::box) {
		manifold = generate_manifold(*lo_bb, *hi_bb, n, separation);
	} else if (lo_type == st::box && hi_type == st::sphere) {
		manifold = box_sphere_manifold(*lo_bb, hi_bb->center(), hi_radius, n, separation);
	} else if (lo_type == st::box && hi_type == st::capsule) {
		manifold = box_capsule_manifold(*lo_bb, *hi_bb, hi_half_height, hi_radius, n, separation);
	} else if (lo_type == st::sphere && hi_type == st::sphere) {
		manifold = sphere_sphere_manifold(lo_bb->center(), lo_radius, hi_bb->center(), hi_radius, n, separation);
	} else if (lo_type == st::sphere && hi_type == st::capsule) {
		manifold = sphere_capsule_manifold(lo_bb->center(), lo_radius, *hi_bb, hi_half_height, hi_radius, n, separation);
	} else if (lo_type == st::capsule && hi_type == st::capsule) {
		manifold = capsule_capsule_manifold(*lo_bb, lo_half_height, lo_radius, *hi_bb, hi_half_height, hi_radius, n, separation);
	}

	if (swap) {
		for (std::uint32_t i = 0; i < manifold.point_count; ++i) {
			std::swap(manifold.points[i].position_on_a, manifold.points[i].position_on_b);
			manifold.points[i].normal = -manifold.points[i].normal;
			auto& [type_a, type_b, index_a, index_b, side_a0, side_a1, side_b0, side_b1] = manifold.points[i].feature;
			std::swap(type_a, type_b);
			std::swap(index_a, index_b);
			std::swap(side_a0, side_b0);
			std::swap(side_a1, side_b1);
		}
		auto [tu, tv] = compute_tangent_basis(normal);
		manifold.tangent_u = tu;
		manifold.tangent_v = tv;
	}

	return manifold;
}
