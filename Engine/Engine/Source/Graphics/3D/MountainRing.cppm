export module gse.graphics:mountain_ring;

import std;

import :mesh;
import :primitive_specs;

import gse.math;

export namespace gse {
	auto build_mountain_ring_meshes(
		const primitive_mountain_ring_spec& spec
	) -> std::vector<mesh_data>;
}
