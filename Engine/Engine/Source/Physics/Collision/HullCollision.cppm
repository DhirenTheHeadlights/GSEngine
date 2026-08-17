export module gse.physics:hull_collision;

import std;

import :bounding_box;
import :contact_manifold;
import :convex_hull;
import :transform_component;

import gse.math;

export namespace gse::physics {
	struct polytope {
		std::span<const vec3<length>> vertices;
		std::span<const vec3f> face_normals;
		std::span<const length> face_offsets;
		std::span<const std::uint8_t> face_first;
		std::span<const std::uint8_t> face_count;
		std::span<const std::uint8_t> face_loop;
		std::span<const std::uint8_t> edge_a;
		std::span<const std::uint8_t> edge_b;
	};

	struct box_polytope {
		std::array<vec3<length>, 8> vertices;
		std::array<vec3f, 6> normals;
		std::array<length, 6> offsets;
		std::array<std::uint8_t, 6> first;
		std::array<std::uint8_t, 6> count;
		std::array<std::uint8_t, 24> loop;
		std::array<std::uint8_t, 12> edge_lo;
		std::array<std::uint8_t, 12> edge_hi;

		auto view() const -> polytope;
	};

	auto make_box_polytope(
		const vec3<displacement>& half_extents
	) -> box_polytope;

	auto hull_polytope(
		const convex_hull& hull
	) -> polytope;

	struct hull_pose {
		vec3<position> center;
		quat orientation;
		std::uint32_t body_index = 0;
	};

	struct hull_sat {
		bool hit = false;
		vec3f normal;
		separation separation;
		bool is_speculative = false;
	};

	auto polytope_sat(
		const polytope& a,
		const hull_pose& pose_a,
		const polytope& b,
		const hull_pose& pose_b,
		length margin
	) -> hull_sat;

	constexpr float reference_face_preference_margin = 0.02f;

	auto polytope_manifold(
		const polytope& a,
		const hull_pose& pose_a,
		const polytope& b,
		const hull_pose& pose_b,
		const vec3f& normal
	) -> contact_manifold;

	struct polytope_point_query {
		vec3<position> closest;
		vec3f normal;
		length signed_distance;
		std::uint8_t face_index;
	};

	auto query_polytope(
		const polytope& p,
		const hull_pose& pose,
		const vec3<position>& point
	) -> polytope_point_query;

	auto segment_polytope_query(
		const polytope& p,
		const hull_pose& pose,
		const vec3<position>& seg_start,
		const vec3<position>& seg_end,
		vec3<position>& best_point
	) -> polytope_point_query;
}

namespace gse::physics {
	constexpr std::size_t max_clip_vertices = 16;

	struct hull_clip_sides {
		std::array<std::uint8_t, 2> values = { feature_side_none, feature_side_none };
		std::uint8_t count = 0;
	};

	struct hull_clip_vertex {
		vec3<position> point;
		hull_clip_sides reference_sides;
		hull_clip_sides incident_sides;
	};

	struct hull_clip_buffer {
		std::array<hull_clip_vertex, max_clip_vertices> vertices;
		std::size_t count = 0;
	};

	struct hull_manifold_candidate {
		vec3<position> position_on_a;
		vec3<position> position_on_b;
		feature_id feature;
	};

	constexpr float parallel_axis_threshold = 1.f - 1e-6f;

	struct polytope_frame {
		std::array<vec3<position>, max_hull_vertices> vertices;
		std::array<vec3f, max_hull_faces> normals;
		std::array<vec3f, max_hull_edges> edges;
		std::uint8_t vertex_count = 0;
		std::uint8_t normal_count = 0;
		std::uint8_t edge_count = 0;
	};

	auto world_vertex(
		const polytope& p,
		const hull_pose& pose,
		std::uint8_t index
	) -> vec3<position>;

	auto world_normal(
		const hull_pose& pose,
		const vec3f& local_normal
	) -> vec3f;

	auto push_unique_axis(
		std::span<vec3f> axes,
		std::uint8_t& count,
		const vec3f& raw_axis
	) -> void;

	auto build_polytope_frame(
		const polytope& p,
		const hull_pose& pose,
		polytope_frame& frame
	) -> void;

	auto span_extreme(
		std::span<const vec3<position>> vertices,
		const vec3f& axis,
		position& lo,
		position& hi
	) -> void;

	auto best_face_along(
		const polytope& p,
		const hull_pose& pose,
		const vec3f& direction
	) -> std::uint8_t;

	auto face_centroid(
		const polytope& p,
		const hull_pose& pose,
		std::uint8_t face
	) -> vec3<position>;

	auto closest_on_segment(
		const vec3<position>& a,
		const vec3<position>& b,
		const vec3<position>& p
	) -> vec3<position>;

	auto add_clip_side(
		hull_clip_sides& sides,
		std::uint8_t side
	) -> void;

	auto shared_clip_sides(
		const hull_clip_sides& lhs,
		const hull_clip_sides& rhs
	) -> hull_clip_sides;

	auto clip_sides_type(
		const hull_clip_sides& sides
	) -> feature_type;

	auto push_clip_vertex(
		hull_clip_buffer& buffer,
		const hull_clip_vertex& vertex
	) -> void;

	auto incident_vertex_sides(
		std::uint8_t ordinal,
		std::uint8_t count
	) -> hull_clip_sides;

	auto clip_against_reference_edge(
		const hull_clip_buffer& subject,
		const vec3f& plane_normal,
		const vec3<position>& plane_point,
		length epsilon,
		std::uint8_t reference_side
	) -> hull_clip_buffer;

	auto clip_vertex_feature(
		const hull_clip_vertex& vertex,
		bool reference_is_a,
		std::uint8_t reference_face,
		std::uint8_t incident_face
	) -> feature_id;
}

auto gse::physics::box_polytope::view() const -> polytope {
	return {
		.vertices = vertices,
		.face_normals = normals,
		.face_offsets = offsets,
		.face_first = first,
		.face_count = count,
		.face_loop = loop,
		.edge_a = edge_lo,
		.edge_b = edge_hi
	};
}

auto gse::physics::make_box_polytope(const vec3<displacement>& half_extents) -> box_polytope {
	box_polytope box{};

	for (std::uint8_t i = 0; i < 8; ++i) {
		box.vertices[i] = vec3<length>(
			(i & 1) ? half_extents.x() : -half_extents.x(),
			(i & 2) ? half_extents.y() : -half_extents.y(),
			(i & 4) ? half_extents.z() : -half_extents.z()
		);
	}

	box.normals = {
		vec3f(1.f, 0.f, 0.f), vec3f(-1.f, 0.f, 0.f),
		vec3f(0.f, 1.f, 0.f), vec3f(0.f, -1.f, 0.f),
		vec3f(0.f, 0.f, 1.f), vec3f(0.f, 0.f, -1.f)
	};

	box.offsets = {
		half_extents.x(), half_extents.x(),
		half_extents.y(), half_extents.y(),
		half_extents.z(), half_extents.z()
	};

	static constexpr std::array<std::uint8_t, 24> face_loops = {
		1, 3, 7, 5,
		0, 4, 6, 2,
		2, 6, 7, 3,
		0, 1, 5, 4,
		4, 5, 7, 6,
		0, 2, 3, 1
	};
	box.loop = face_loops;

	for (std::uint8_t f = 0; f < 6; ++f) {
		box.first[f] = static_cast<std::uint8_t>(f * 4);
		box.count[f] = 4;
	}

	static constexpr std::array<std::uint8_t, 12> lo = { 0, 2, 4, 6, 0, 1, 4, 5, 0, 1, 2, 3 };
	static constexpr std::array<std::uint8_t, 12> hi = { 1, 3, 5, 7, 2, 3, 6, 7, 4, 5, 6, 7 };
	box.edge_lo = lo;
	box.edge_hi = hi;

	return box;
}

auto gse::physics::hull_polytope(const convex_hull& hull) -> polytope {
	return {
		.vertices = hull.vertices,
		.face_normals = hull.face_normals,
		.face_offsets = hull.face_offsets,
		.face_first = hull.face_first,
		.face_count = hull.face_count,
		.face_loop = hull.face_loop,
		.edge_a = hull.edge_a,
		.edge_b = hull.edge_b
	};
}

auto gse::physics::world_vertex(const polytope& p, const hull_pose& pose, const std::uint8_t index) -> vec3<position> {
	return pose.center + rotate_vector(pose.orientation, p.vertices[index]);
}

auto gse::physics::world_normal(const hull_pose& pose, const vec3f& local_normal) -> vec3f {
	return rotate_vector(pose.orientation, local_normal);
}

auto gse::physics::push_unique_axis(const std::span<vec3f> axes, std::uint8_t& count, const vec3f& raw_axis) -> void {
	const float axis_len = magnitude(raw_axis);
	if (axis_len < 1e-6f) {
		return;
	}

	const vec3f axis = raw_axis / axis_len;
	for (std::uint8_t i = 0; i < count; ++i) {
		if (std::abs(dot(axes[i], axis)) > parallel_axis_threshold) {
			return;
		}
	}

	if (count >= axes.size()) {
		return;
	}
	axes[count++] = axis;
}

auto gse::physics::build_polytope_frame(const polytope& p, const hull_pose& pose, polytope_frame& frame) -> void {
	frame.vertex_count = static_cast<std::uint8_t>(std::min(p.vertices.size(), max_hull_vertices));
	for (std::uint8_t i = 0; i < frame.vertex_count; ++i) {
		frame.vertices[i] = pose.center + rotate_vector(pose.orientation, p.vertices[i]);
	}

	for (std::size_t f = 0; f < p.face_normals.size(); ++f) {
		push_unique_axis(frame.normals, frame.normal_count, world_normal(pose, p.face_normals[f]));
	}

	const auto degenerate_edge = meters(1e-6f);
	for (std::size_t e = 0; e < p.edge_a.size(); ++e) {
		const auto lo = p.edge_a[e];
		const auto hi = p.edge_b[e];
		if (lo >= frame.vertex_count || hi >= frame.vertex_count) {
			continue;
		}
		if (const auto delta = frame.vertices[hi] - frame.vertices[lo]; magnitude(delta) > degenerate_edge) {
			push_unique_axis(frame.edges, frame.edge_count, normalize(delta));
		}
	}
}

auto gse::physics::span_extreme(const std::span<const vec3<position>> vertices, const vec3f& axis, position& lo, position& hi) -> void {
	lo = dot(axis, vertices[0]);
	hi = lo;
	for (std::size_t i = 1; i < vertices.size(); ++i) {
		const auto d = dot(axis, vertices[i]);
		lo = std::min(lo, d);
		hi = std::max(hi, d);
	}
}

auto gse::physics::face_centroid(const polytope& p, const hull_pose& pose, const std::uint8_t face) -> vec3<position> {
	const auto first = p.face_first[face];
	const auto count = p.face_count[face];
	const auto anchor = world_vertex(p, pose, p.face_loop[first]);

	auto center = anchor;
	for (std::uint8_t i = 1; i < count; ++i) {
		center += (world_vertex(p, pose, p.face_loop[first + i]) - anchor) / static_cast<float>(count);
	}
	return center;
}

auto gse::physics::best_face_along(const polytope& p, const hull_pose& pose, const vec3f& direction) -> std::uint8_t {
	std::uint8_t best = 0;
	float best_dot = -std::numeric_limits<float>::max();
	for (std::uint8_t f = 0; f < p.face_normals.size(); ++f) {
		if (const float d = dot(world_normal(pose, p.face_normals[f]), direction); d > best_dot) {
			best_dot = d;
			best = f;
		}
	}
	return best;
}

auto gse::physics::closest_on_segment(const vec3<position>& a, const vec3<position>& b, const vec3<position>& p) -> vec3<position> {
	const auto ab = b - a;
	if (magnitude(ab) < meters(1e-8f)) {
		return a;
	}
	const float t = std::clamp<float>(dot(p - a, ab) / dot(ab, ab), 0.f, 1.f);
	return a + ab * t;
}

auto gse::physics::add_clip_side(hull_clip_sides& sides, const std::uint8_t side) -> void {
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
	std::ranges::sort(sides.values.begin(), sides.values.begin() + sides.count);
}

auto gse::physics::shared_clip_sides(const hull_clip_sides& lhs, const hull_clip_sides& rhs) -> hull_clip_sides {
	hull_clip_sides shared;
	for (std::uint8_t i = 0; i < lhs.count; ++i) {
		for (std::uint8_t j = 0; j < rhs.count; ++j) {
			if (lhs.values[i] == rhs.values[j]) {
				add_clip_side(shared, lhs.values[i]);
			}
		}
	}
	return shared;
}

auto gse::physics::clip_sides_type(const hull_clip_sides& sides) -> feature_type {
	return sides.count == 0 ? feature_type::face : (sides.count == 1 ? feature_type::edge : feature_type::vertex);
}

auto gse::physics::push_clip_vertex(hull_clip_buffer& buffer, const hull_clip_vertex& vertex) -> void {
	if (buffer.count >= buffer.vertices.size()) {
		return;
	}
	buffer.vertices[buffer.count++] = vertex;
}

auto gse::physics::incident_vertex_sides(const std::uint8_t ordinal, const std::uint8_t count) -> hull_clip_sides {
	hull_clip_sides sides;
	if (count == 0) {
		return sides;
	}
	add_clip_side(sides, static_cast<std::uint8_t>((ordinal + count - 1) % count));
	add_clip_side(sides, ordinal);
	return sides;
}

auto gse::physics::clip_against_reference_edge(const hull_clip_buffer& subject, const vec3f& plane_normal, const vec3<position>& plane_point, const length epsilon, const std::uint8_t reference_side) -> hull_clip_buffer {
	hull_clip_buffer clipped;
	if (subject.count == 0) {
		return clipped;
	}

	const auto inside = [&](const length signed_distance) {
		return signed_distance <= epsilon;
	};

	for (std::size_t i = 0; i < subject.count; ++i) {
		const auto& current = subject.vertices[i];
		const auto& next = subject.vertices[(i + 1) % subject.count];
		const auto d_current = dot(plane_normal, current.point - plane_point);
		const auto d_next = dot(plane_normal, next.point - plane_point);
		const bool current_inside = inside(d_current);
		const bool next_inside = inside(d_next);

		if (current_inside) {
			auto retained = current;
			if (abs(d_current) <= epsilon) {
				add_clip_side(retained.reference_sides, reference_side);
			}
			push_clip_vertex(clipped, retained);
		}

		if (current_inside != next_inside) {
			const auto denominator = d_current - d_next;
			if (abs(denominator) > meters(1e-6f)) {
				const float t = d_current / denominator;
				hull_clip_vertex intersection{
					.point = current.point + (next.point - current.point) * t,
					.reference_sides = shared_clip_sides(current.reference_sides, next.reference_sides),
					.incident_sides = shared_clip_sides(current.incident_sides, next.incident_sides)
				};
				add_clip_side(intersection.reference_sides, reference_side);
				push_clip_vertex(clipped, intersection);
			}
		}
	}

	return clipped;
}

auto gse::physics::clip_vertex_feature(const hull_clip_vertex& vertex, const bool reference_is_a, const std::uint8_t reference_face, const std::uint8_t incident_face) -> feature_id {
	const auto reference_type = clip_sides_type(vertex.reference_sides);
	const auto incident_type = clip_sides_type(vertex.incident_sides);

	if (reference_is_a) {
		return {
			.type_a = reference_type,
			.type_b = incident_type,
			.index_a = reference_face,
			.index_b = incident_face,
			.side_a0 = vertex.reference_sides.values[0],
			.side_a1 = vertex.reference_sides.values[1],
			.side_b0 = vertex.incident_sides.values[0],
			.side_b1 = vertex.incident_sides.values[1]
		};
	}

	return {
		.type_a = incident_type,
		.type_b = reference_type,
		.index_a = incident_face,
		.index_b = reference_face,
		.side_a0 = vertex.incident_sides.values[0],
		.side_a1 = vertex.incident_sides.values[1],
		.side_b0 = vertex.reference_sides.values[0],
		.side_b1 = vertex.reference_sides.values[1]
	};
}

auto gse::physics::polytope_sat(const polytope& a, const hull_pose& pose_a, const polytope& b, const hull_pose& pose_b, const length margin) -> hull_sat {
	hull_sat result;

	separation best_overlap{};
	vec3f best_axis;
	bool found = false;
	bool all_positive = true;

	polytope_frame frame_a;
	polytope_frame frame_b;
	build_polytope_frame(a, pose_a, frame_a);
	build_polytope_frame(b, pose_b, frame_b);

	if (frame_a.vertex_count == 0 || frame_b.vertex_count == 0) {
		return result;
	}

	const auto vertices_a = std::span<const vec3<position>>(frame_a.vertices).first(frame_a.vertex_count);
	const auto vertices_b = std::span<const vec3<position>>(frame_b.vertices).first(frame_b.vertex_count);

	const auto test_axis = [&](const vec3f& axis) -> bool {
		position lo_a;
		position hi_a;
		position lo_b;
		position hi_b;
		span_extreme(vertices_a, axis, lo_a, hi_a);
		span_extreme(vertices_b, axis, lo_b, hi_b);

		const separation overlap = std::min(hi_a - lo_b, hi_b - lo_a);
		if (overlap < -margin) {
			return false;
		}
		if (overlap <= separation{}) {
			all_positive = false;
		}
		if (!found || overlap < best_overlap) {
			found = true;
			best_overlap = overlap;
			best_axis = axis;
		}
		return true;
	};

	for (std::uint8_t f = 0; f < frame_a.normal_count; ++f) {
		if (!test_axis(frame_a.normals[f])) {
			return result;
		}
	}
	for (std::uint8_t f = 0; f < frame_b.normal_count; ++f) {
		if (!test_axis(frame_b.normals[f])) {
			return result;
		}
	}

	for (std::uint8_t i = 0; i < frame_a.edge_count; ++i) {
		for (std::uint8_t j = 0; j < frame_b.edge_count; ++j) {
			const auto raw_axis = cross(frame_a.edges[i], frame_b.edges[j]);
			const float axis_len = magnitude(raw_axis);
			if (axis_len < 1e-6f) {
				continue;
			}
			if (!test_axis(raw_axis / axis_len)) {
				return result;
			}
		}
	}

	if (!found) {
		return result;
	}

	if (dot(best_axis, pose_b.center - pose_a.center) < meters(0.f)) {
		best_axis = -best_axis;
	}

	result.hit = true;
	result.normal = best_axis;
	result.separation = best_overlap;
	result.is_speculative = !all_positive;
	return result;
}

auto gse::physics::query_polytope(const polytope& p, const hull_pose& pose, const vec3<position>& point) -> polytope_point_query {
	polytope_point_query result{};

	auto deepest = -meters(std::numeric_limits<float>::max());
	std::uint8_t deepest_face = 0;
	bool outside = false;

	for (std::uint8_t f = 0; f < p.face_normals.size(); ++f) {
		const auto normal = world_normal(pose, p.face_normals[f]);
		const auto plane_point = pose.center + rotate_vector(pose.orientation, p.vertices[p.face_loop[p.face_first[f]]]);
		const auto distance = dot(normal, point - plane_point);
		if (distance > meters(0.f)) {
			outside = true;
		}
		if (distance > deepest) {
			deepest = distance;
			deepest_face = f;
		}
	}

	result.face_index = deepest_face;

	if (!outside) {
		result.normal = world_normal(pose, p.face_normals[deepest_face]);
		result.closest = point - result.normal * deepest;
		result.signed_distance = deepest;
		return result;
	}

	auto best_distance = meters(std::numeric_limits<float>::max());
	vec3<position> best_point = point;
	std::uint8_t best_face = deepest_face;

	for (std::uint8_t f = 0; f < p.face_normals.size(); ++f) {
		const auto first = p.face_first[f];
		const auto count = p.face_count[f];
		const auto normal = world_normal(pose, p.face_normals[f]);
		const auto anchor = world_vertex(p, pose, p.face_loop[first]);
		if (dot(normal, point - anchor) <= meters(0.f)) {
			continue;
		}

		auto projected = point - normal * dot(normal, point - anchor);
		bool inside_face = true;
		for (std::uint8_t i = 0; i < count; ++i) {
			const auto v0 = world_vertex(p, pose, p.face_loop[first + i]);
			const auto v1 = world_vertex(p, pose, p.face_loop[first + (i + 1) % count]);
			if (dot(cross(v1 - v0, projected - v0), normal) < meters(0.f) * meters(0.f)) {
				inside_face = false;
			}
		}

		if (!inside_face) {
			projected = point;
			auto edge_best = meters(std::numeric_limits<float>::max());
			for (std::uint8_t i = 0; i < count; ++i) {
				const auto v0 = world_vertex(p, pose, p.face_loop[first + i]);
				const auto v1 = world_vertex(p, pose, p.face_loop[first + (i + 1) % count]);
				const auto candidate = closest_on_segment(v0, v1, point);
				if (const auto d = magnitude(point - candidate); d < edge_best) {
					edge_best = d;
					projected = candidate;
				}
			}
		}

		if (const auto d = magnitude(point - projected); d < best_distance) {
			best_distance = d;
			best_point = projected;
			best_face = f;
		}
	}

	const auto to_point = point - best_point;
	const auto dist = magnitude(to_point);

	result.closest = best_point;
	result.normal = (dist > meters(1e-6f)) ? normalize(to_point) : world_normal(pose, p.face_normals[best_face]);
	result.signed_distance = dist;
	result.face_index = best_face;
	return result;
}

auto gse::physics::segment_polytope_query(const polytope& p, const hull_pose& pose, const vec3<position>& seg_start, const vec3<position>& seg_end, vec3<position>& best_point) -> polytope_point_query {
	auto probe = closest_on_segment(seg_start, seg_end, pose.center);
	auto best = query_polytope(p, pose, probe);
	best_point = probe;

	for (int iteration = 0; iteration < 3; ++iteration) {
		probe = closest_on_segment(seg_start, seg_end, best.closest);
		if (const auto candidate = query_polytope(p, pose, probe); candidate.signed_distance < best.signed_distance) {
			best = candidate;
			best_point = probe;
		}
	}

	if (const auto at_start = query_polytope(p, pose, seg_start); at_start.signed_distance < best.signed_distance) {
		best = at_start;
		best_point = seg_start;
	}

	if (const auto at_end = query_polytope(p, pose, seg_end); at_end.signed_distance < best.signed_distance) {
		best = at_end;
		best_point = seg_end;
	}

	return best;
}

auto gse::physics::polytope_manifold(const polytope& a, const hull_pose& pose_a, const polytope& b, const hull_pose& pose_b, const vec3f& normal) -> contact_manifold {
	contact_manifold manifold;
	auto [tu, tv] = compute_tangent_basis(normal);
	manifold.tangent_u = tu;
	manifold.tangent_v = tv;

	const auto face_a = best_face_along(a, pose_a, normal);
	const auto face_b = best_face_along(b, pose_b, -normal);

	const float align_a = dot(world_normal(pose_a, a.face_normals[face_a]), normal);
	const float align_b = dot(world_normal(pose_b, b.face_normals[face_b]), -normal);

	const bool reference_is_a = std::abs(align_a - align_b) <= reference_face_preference_margin
		? pose_a.body_index < pose_b.body_index
		: align_a > align_b;

	const auto& reference = reference_is_a ? a : b;
	const auto& incident = reference_is_a ? b : a;
	const auto& reference_pose = reference_is_a ? pose_a : pose_b;
	const auto& incident_pose = reference_is_a ? pose_b : pose_a;
	const auto reference_face = reference_is_a ? face_a : face_b;
	const auto incident_face = reference_is_a ? face_b : face_a;
	const auto reference_normal = reference_is_a ? normal : -normal;

	const auto ref_first = reference.face_first[reference_face];
	const auto ref_count = reference.face_count[reference_face];
	const auto inc_first = incident.face_first[incident_face];
	const auto inc_count = incident.face_count[incident_face];

	const auto clip_epsilon = meters(1e-4f);
	const auto degenerate_edge_epsilon = meters(1e-6f);

	hull_clip_buffer polygon;
	for (std::uint8_t i = 0; i < inc_count; ++i) {
		push_clip_vertex(
			polygon,
			hull_clip_vertex{
				.point = world_vertex(incident, incident_pose, incident.face_loop[inc_first + i]),
				.incident_sides = incident_vertex_sides(i, inc_count)
			}
		);
	}

	for (std::uint8_t i = 0; i < ref_count; ++i) {
		const auto v0 = world_vertex(reference, reference_pose, reference.face_loop[ref_first + i]);
		const auto v1 = world_vertex(reference, reference_pose, reference.face_loop[ref_first + (i + 1) % ref_count]);
		const auto plane_axis = cross(v1 - v0, reference_normal);
		if (magnitude(plane_axis) < degenerate_edge_epsilon) {
			continue;
		}

		polygon = clip_against_reference_edge(polygon, normalize(plane_axis), v0, clip_epsilon, i);
		if (polygon.count == 0) {
			return manifold;
		}
	}

	const auto ref_anchor = world_vertex(reference, reference_pose, reference.face_loop[ref_first]);

	std::array<hull_manifold_candidate, max_clip_vertices> candidates;
	std::size_t candidate_count = 0;

	for (std::size_t i = 0; i < polygon.count; ++i) {
		const auto& vertex = polygon.vertices[i];
		const auto depth = dot(reference_normal, vertex.point - ref_anchor);
		if (depth > clip_epsilon) {
			continue;
		}

		const auto on_reference = vertex.point - reference_normal * depth;
		candidates[candidate_count++] = {
			.position_on_a = reference_is_a ? on_reference : vertex.point,
			.position_on_b = reference_is_a ? vertex.point : on_reference,
			.feature = clip_vertex_feature(vertex, reference_is_a, reference_face, incident_face)
		};
	}

	const auto midpoint = [](const hull_manifold_candidate& candidate) {
		return candidate.position_on_b + (candidate.position_on_a - candidate.position_on_b) * 0.5f;
	};

	const auto quantize = [](const length value) -> int {
		const float scaled = value / meters(1e-4f);
		return static_cast<int>(std::floor(scaled + (scaled >= 0.f ? 0.5f : -0.5f)));
	};

	std::ranges::sort(
		std::span(candidates).first(candidate_count),
		[&](const hull_manifold_candidate& lhs, const hull_manifold_candidate& rhs) {
			const std::uint64_t lhs_key = pack_feature(lhs.feature);
			const std::uint64_t rhs_key = pack_feature(rhs.feature);
			if (lhs_key != rhs_key) {
				return lhs_key < rhs_key;
			}

			const int lhs_u = quantize(dot(midpoint(lhs) - pose_a.center, manifold.tangent_u));
			const int rhs_u = quantize(dot(midpoint(rhs) - pose_a.center, manifold.tangent_u));
			if (lhs_u != rhs_u) {
				return lhs_u < rhs_u;
			}

			const int lhs_v = quantize(dot(midpoint(lhs) - pose_a.center, manifold.tangent_v));
			const int rhs_v = quantize(dot(midpoint(rhs) - pose_a.center, manifold.tangent_v));
			if (lhs_v != rhs_v) {
				return lhs_v < rhs_v;
			}

			return dot(midpoint(lhs) - pose_a.center, normal) < dot(midpoint(rhs) - pose_a.center, normal);
		}
	);

	for (std::size_t i = 0; i < candidate_count; ++i) {
		if (manifold.point_count >= 4) {
			break;
		}
		if (i > 0 && candidates[i].feature == candidates[i - 1].feature) {
			continue;
		}

		manifold.add_point(
			contact_point{
				.position_on_a = candidates[i].position_on_a,
				.position_on_b = candidates[i].position_on_b,
				.normal = normal,
				.separation = meters(0.f),
				.feature = candidates[i].feature
			}
		);
	}

	if (manifold.point_count == 0) {
		manifold.add_point(
			contact_point{
				.position_on_a = face_centroid(a, pose_a, face_a),
				.position_on_b = face_centroid(b, pose_b, face_b),
				.normal = normal,
				.separation = meters(0.f),
				.feature =
					feature_id{
						.type_a = feature_type::face,
						.type_b = feature_type::face,
						.index_a = face_a,
						.index_b = face_b,
					}
			}
		);
	}

	return manifold;
}
