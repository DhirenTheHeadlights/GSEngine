export module gse.graphics:hull_mesh;

import std;

import :mesh;
import :primitive_specs;

import gse.math;

export namespace gse {
	auto build_hull_meshes(
		const surface_spec& surface,
		std::span<const vec3<length>> points
	) -> std::vector<mesh_data>;
}
