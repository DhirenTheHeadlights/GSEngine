module gse.graphics:hull_mesh_impl;

import std;

import :hull_mesh;
import :material;
import :mesh;
import :primitive_specs;

import gse.assert;
import gse.math;
import gse.physics;

auto gse::build_hull_meshes(const surface_spec& surface, const std::span<const vec3<length>> points) -> std::vector<mesh_data> {
	const auto hull = physics::build_convex_hull(points);
	assert(hull.valid(), "a hull primitive needs a valid convex hull, but {} points produced none", points.size());

	std::vector<vertex> vertices;
	vertices.reserve(hull.face_loop.size());
	std::vector<std::uint32_t> indices;
	indices.reserve((hull.face_loop.size() - 2 * hull.face_normals.size()) * 3);

	for (std::size_t face = 0; face < hull.face_normals.size(); ++face) {
		const auto first = hull.face_first[face];
		const auto count = hull.face_count[face];
		const auto base = static_cast<std::uint32_t>(vertices.size());
		for (std::size_t i = 0; i < count; ++i) {
			vertices.push_back({
				.position = hull.vertices[hull.face_loop[first + i]],
				.normal = hull.face_normals[face],
				.tex_coords = vec2f(0.0f),
			});
		}
		for (std::uint32_t i = 1; i + 1 < count; ++i) {
			indices.push_back(base);
			indices.push_back(base + i);
			indices.push_back(base + i + 1);
		}
	}

	std::vector<mesh_data> meshes;
	meshes.push_back({
		.vertices = std::move(vertices),
		.indices = std::move(indices),
		.material = {
			.base_color = surface.base_color,
			.roughness = surface.roughness,
			.metallic = surface.metallic,
		},
	});
	return meshes;
}
