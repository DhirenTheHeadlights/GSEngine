export module gse.physics:convex_hull;

import std;

import gse.math;

export namespace gse::physics {
	using hull_quartic = decltype(volume{} * length{});
	using hull_quintic = decltype(volume{} * length{} * length{});

	constexpr std::size_t max_hull_vertices = 64;
	constexpr std::size_t max_hull_faces = 128;
	constexpr std::size_t max_hull_edges = 192;

	struct convex_hull {
		std::vector<vec3<length>> vertices;
		std::vector<vec3f> face_normals;
		std::vector<length> face_offsets;
		std::vector<std::uint8_t> face_first;
		std::vector<std::uint8_t> face_count;
		std::vector<std::uint8_t> face_loop;
		std::vector<std::uint8_t> edge_a;
		std::vector<std::uint8_t> edge_b;

		auto valid() const -> bool;
	};

	struct hull_mass_terms {
		volume total_volume;
		vec3<displacement> centroid;
		mat3<inertia> unit_density_tensor;
	};

	auto build_convex_hull(
		std::span<const vec3<length>> points
	) -> convex_hull;

	auto integrate_hull(
		const convex_hull& hull
	) -> hull_mass_terms;

	auto hull_support(
		const convex_hull& hull,
		const vec3f& direction
	) -> vec3<length>;

	auto hull_half_extents(
		const convex_hull& hull
	) -> vec3<displacement>;
}

namespace gse::physics {
	struct hull_face_scratch {
		std::array<std::uint8_t, 3> v;
		vec3f normal;
		length offset;
		bool live;
	};

	auto plane_of(
		const vec3<length>& a,
		const vec3<length>& b,
		const vec3<length>& c,
		vec3f& normal,
		length& offset
	) -> bool;

	auto seed_tetrahedron(
		std::span<const vec3<length>> points,
		std::array<std::uint8_t, 4>& seed
	) -> bool;

	auto push_oriented_face(
		std::vector<hull_face_scratch>& out,
		const std::vector<vec3<length>>& verts,
		std::uint8_t i0,
		std::uint8_t i1,
		std::uint8_t i2,
		const vec3<length>& interior
	) -> void;
}

auto gse::physics::push_oriented_face(std::vector<hull_face_scratch>& out, const std::vector<vec3<length>>& verts, const std::uint8_t i0, const std::uint8_t i1, const std::uint8_t i2, const vec3<length>& interior) -> void {
	vec3f normal;
	length offset;
	if (!plane_of(verts[i0], verts[i1], verts[i2], normal, offset)) {
		return;
	}
	if (dot(normal, interior) - offset > meters(0.f)) {
		normal = -normal;
		offset = dot(normal, verts[i0]);
		out.push_back({ .v = { i0, i2, i1 }, .normal = normal, .offset = offset, .live = true });
		return;
	}
	out.push_back({ .v = { i0, i1, i2 }, .normal = normal, .offset = offset, .live = true });
}


auto gse::physics::convex_hull::valid() const -> bool {
	return vertices.size() >= 4 && !face_normals.empty() && !edge_a.empty();
}

auto gse::physics::plane_of(const vec3<length>& a, const vec3<length>& b, const vec3<length>& c, vec3f& normal, length& offset) -> bool {
	const auto n = cross(b - a, c - a);
	const auto area = magnitude(n);
	if (area <= meters(1e-9f) * meters(1e-9f)) {
		return false;
	}
	normal = normalize(n);
	offset = dot(normal, a);
	return true;
}

auto gse::physics::seed_tetrahedron(const std::span<const vec3<length>> points, std::array<std::uint8_t, 4>& seed) -> bool {
	if (points.size() < 4) {
		return false;
	}

	std::size_t min_x = 0;
	std::size_t max_x = 0;
	for (std::size_t i = 1; i < points.size(); ++i) {
		if (points[i].x() < points[min_x].x()) {
			min_x = i;
		}
		if (points[i].x() > points[max_x].x()) {
			max_x = i;
		}
	}
	if (min_x == max_x) {
		return false;
	}

	const auto base_a = points[min_x];
	const auto base_b = points[max_x];

	std::size_t best_line = 0;
	auto best_line_area = meters(0.f) * meters(0.f);
	for (std::size_t i = 0; i < points.size(); ++i) {
		const auto area = magnitude(cross(base_b - base_a, points[i] - base_a));
		if (area > best_line_area) {
			best_line_area = area;
			best_line = i;
		}
	}
	if (best_line_area <= meters(1e-9f) * meters(1e-9f)) {
		return false;
	}

	vec3f plane_normal;
	length plane_offset;
	if (!plane_of(base_a, base_b, points[best_line], plane_normal, plane_offset)) {
		return false;
	}

	std::size_t best_apex = 0;
	auto best_depth = meters(0.f);
	for (std::size_t i = 0; i < points.size(); ++i) {
		if (const auto depth = abs(dot(plane_normal, points[i]) - plane_offset); depth > best_depth) {
			best_depth = depth;
			best_apex = i;
		}
	}
	if (best_depth <= meters(1e-7f)) {
		return false;
	}

	seed = {
		static_cast<std::uint8_t>(min_x),
		static_cast<std::uint8_t>(max_x),
		static_cast<std::uint8_t>(best_line),
		static_cast<std::uint8_t>(best_apex)
	};
	return true;
}

auto gse::physics::build_convex_hull(const std::span<const vec3<length>> points) -> convex_hull {
	convex_hull hull;

	std::array<std::uint8_t, 4> seed{};
	if (!seed_tetrahedron(points, seed)) {
		return hull;
	}

	std::vector<vec3<length>> verts;
	verts.reserve(std::min(points.size(), max_hull_vertices));
	for (const auto index : seed) {
		verts.push_back(points[index]);
	}

	std::vector<hull_face_scratch> faces;

	const auto interior = (verts[0] + verts[1] + verts[2] + verts[3]) * 0.25f;
	push_oriented_face(faces, verts, 0, 1, 2, interior);
	push_oriented_face(faces, verts, 0, 1, 3, interior);
	push_oriented_face(faces, verts, 0, 2, 3, interior);
	push_oriented_face(faces, verts, 1, 2, 3, interior);
	if (faces.size() != 4) {
		return hull;
	}

	std::vector<std::uint8_t> consumed(points.size(), 0);
	for (const auto index : seed) {
		consumed[index] = 1;
	}

	const auto tolerance = meters(1e-6f);

	for (std::size_t p = 0; p < points.size() && verts.size() < max_hull_vertices; ++p) {
		if (consumed[p]) {
			continue;
		}
		const auto point = points[p];

		bool any_visible = false;
		for (auto& face : faces) {
			if (face.live && dot(face.normal, point) - face.offset > tolerance) {
				face.live = false;
				any_visible = true;
			}
		}
		if (!any_visible) {
			continue;
		}

		std::vector<std::pair<std::uint8_t, std::uint8_t>> horizon;
		for (const auto& face : faces) {
			if (face.live) {
				continue;
			}
			for (std::size_t e = 0; e < 3; ++e) {
				const auto a = face.v[e];
				const auto b = face.v[(e + 1) % 3];
				bool shared_with_live = false;
				for (const auto& other : faces) {
					if (!other.live) {
						continue;
					}
					for (std::size_t oe = 0; oe < 3; ++oe) {
						if (other.v[oe] == b && other.v[(oe + 1) % 3] == a) {
							shared_with_live = true;
						}
					}
				}
				if (shared_with_live) {
					horizon.emplace_back(a, b);
				}
			}
		}

		if (horizon.empty()) {
			for (auto& face : faces) {
				face.live = true;
			}
			continue;
		}

		consumed[p] = 1;
		const auto apex = static_cast<std::uint8_t>(verts.size());
		verts.push_back(point);

		std::erase_if(faces, [](const hull_face_scratch& f) { return !f.live; });

		for (const auto& [a, b] : horizon) {
			push_oriented_face(faces, verts, a, b, apex, interior);
		}

		if (faces.size() > max_hull_faces) {
			return convex_hull{};
		}
	}

	if (faces.size() < 4 || verts.size() < 4) {
		return hull;
	}

	hull.vertices = std::move(verts);

	std::vector<std::uint8_t> face_of_plane(faces.size(), 0xFF);
	for (std::size_t i = 0; i < faces.size(); ++i) {
		if (face_of_plane[i] != 0xFF) {
			continue;
		}
		const auto plane_index = static_cast<std::uint8_t>(hull.face_normals.size());
		hull.face_normals.push_back(faces[i].normal);
		hull.face_offsets.push_back(faces[i].offset);
		for (std::size_t j = i; j < faces.size(); ++j) {
			if (face_of_plane[j] != 0xFF) {
				continue;
			}
			if (dot(faces[j].normal, faces[i].normal) < 0.9999f) {
				continue;
			}
			if (abs(faces[j].offset - faces[i].offset) > meters(1e-5f)) {
				continue;
			}
			face_of_plane[j] = plane_index;
		}
	}

	for (std::size_t plane = 0; plane < hull.face_normals.size(); ++plane) {
		std::vector<std::pair<std::uint8_t, std::uint8_t>> boundary;
		for (std::size_t f = 0; f < faces.size(); ++f) {
			if (face_of_plane[f] != plane) {
				continue;
			}
			for (std::size_t e = 0; e < 3; ++e) {
				const auto a = faces[f].v[e];
				const auto b = faces[f].v[(e + 1) % 3];
				const auto opposite = std::ranges::find_if(
					boundary,
					[&](const std::pair<std::uint8_t, std::uint8_t>& edge) {
						return edge.first == b && edge.second == a;
					}
				);
				if (opposite != boundary.end()) {
					boundary.erase(opposite);
					continue;
				}
				boundary.emplace_back(a, b);
			}
		}

		if (boundary.size() < 3) {
			return convex_hull{};
		}

		hull.face_first.push_back(static_cast<std::uint8_t>(hull.face_loop.size()));
		hull.face_count.push_back(static_cast<std::uint8_t>(boundary.size()));

		auto current = boundary.front().first;
		for (std::size_t step = 0; step < boundary.size(); ++step) {
			hull.face_loop.push_back(current);
			const auto next = std::ranges::find_if(
				boundary,
				[&](const std::pair<std::uint8_t, std::uint8_t>& edge) {
					return edge.first == current;
				}
			);
			if (next == boundary.end()) {
				return convex_hull{};
			}
			current = next->second;
		}
	}

	for (std::size_t plane = 0; plane < hull.face_normals.size(); ++plane) {
		const auto first = hull.face_first[plane];
		const auto count = hull.face_count[plane];
		for (std::size_t i = 0; i < count; ++i) {
			const auto a = hull.face_loop[first + i];
			const auto b = hull.face_loop[first + (i + 1) % count];
			const auto lo = std::min(a, b);
			const auto hi = std::max(a, b);
			bool exists = false;
			for (std::size_t e = 0; e < hull.edge_a.size(); ++e) {
				if (hull.edge_a[e] == lo && hull.edge_b[e] == hi) {
					exists = true;
				}
			}
			if (!exists && hull.edge_a.size() < max_hull_edges) {
				hull.edge_a.push_back(lo);
				hull.edge_b.push_back(hi);
			}
		}
	}

	return hull;
}

auto gse::physics::integrate_hull(const convex_hull& hull) -> hull_mass_terms {
	hull_mass_terms terms{};
	if (!hull.valid()) {
		return terms;
	}

	const auto origin = hull.vertices[0];

	volume volume_sum{};
	vec3<hull_quartic> moment_sum{};
	std::array<hull_quintic, 9> covariance{};

	static constexpr std::array<float, 9> weights = { 2.f, 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 1.f, 2.f };

	for (std::size_t plane = 0; plane < hull.face_normals.size(); ++plane) {
		const auto first = hull.face_first[plane];
		const auto count = hull.face_count[plane];
		for (std::size_t i = 1; i + 1 < count; ++i) {
			const std::array<vec3<length>, 3> w = {
				hull.vertices[hull.face_loop[first]] - origin,
				hull.vertices[hull.face_loop[first + i]] - origin,
				hull.vertices[hull.face_loop[first + i + 1]] - origin
			};

			const volume det = dot(w[0], cross(w[1], w[2]));

			volume_sum += det;
			moment_sum += (w[0] + w[1] + w[2]) * det;

			for (std::size_t r = 0; r < 3; ++r) {
				for (std::size_t c = 0; c < 3; ++c) {
					auto acc = meters(0.f) * meters(0.f);
					for (std::size_t p = 0; p < 3; ++p) {
						for (std::size_t q = 0; q < 3; ++q) {
							acc += w[p][r] * w[q][c] * weights[p * 3 + q];
						}
					}
					covariance[r * 3 + c] += det * acc / 120.f;
				}
			}
		}
	}

	if (volume_sum <= volume{}) {
		return terms;
	}

	const volume hull_volume = volume_sum / 6.f;
	const vec3<length> centroid_local = moment_sum / (volume_sum * 4.f);

	for (std::size_t r = 0; r < 3; ++r) {
		for (std::size_t c = 0; c < 3; ++c) {
			covariance[r * 3 + c] -= hull_volume * centroid_local[r] * centroid_local[c];
		}
	}

	const hull_quintic trace = covariance[0] + covariance[4] + covariance[8];
	const auto unit_density = kilograms_per_cubic_meter(1.f);
	const auto unit_density_per_angle_squared = unit_density / (rad * rad);

	terms.total_volume = hull_volume;
	terms.centroid = vec3<displacement>(centroid_local + origin);

	terms.unit_density_tensor = mat3<inertia>{
		vec3<inertia>((trace - covariance[0]) * unit_density_per_angle_squared, -covariance[3] * unit_density_per_angle_squared, -covariance[6] * unit_density_per_angle_squared),
		vec3<inertia>(-covariance[1] * unit_density_per_angle_squared, (trace - covariance[4]) * unit_density_per_angle_squared, -covariance[7] * unit_density_per_angle_squared),
		vec3<inertia>(-covariance[2] * unit_density_per_angle_squared, -covariance[5] * unit_density_per_angle_squared, (trace - covariance[8]) * unit_density_per_angle_squared)
	};

	return terms;
}

auto gse::physics::hull_support(const convex_hull& hull, const vec3f& direction) -> vec3<length> {
	if (hull.vertices.empty()) {
		return {};
	}
	std::size_t best = 0;
	auto best_dot = dot(direction, hull.vertices[0]);
	for (std::size_t i = 1; i < hull.vertices.size(); ++i) {
		if (const auto d = dot(direction, hull.vertices[i]); d > best_dot) {
			best_dot = d;
			best = i;
		}
	}
	return hull.vertices[best];
}

auto gse::physics::hull_half_extents(const convex_hull& hull) -> vec3<displacement> {
	auto extent = vec3<displacement>();
	for (const auto& v : hull.vertices) {
		for (std::size_t axis = 0; axis < 3; ++axis) {
			if (const auto a = abs(v[axis]); a > extent[axis]) {
				extent[axis] = a;
			}
		}
	}
	return extent;
}
