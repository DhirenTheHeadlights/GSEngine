export module gse.graphics:primitives;

import std;

import gse.assets;

import :mesh;
import :model;

export namespace gse::primitives {
	struct data {
		resource::handle<model> unit_box;
		resource::handle<model> sphere_lo;
		resource::handle<model> sphere_mid;
		resource::handle<model> sphere_hi;
		resource::handle<model> unit_cylinder;
	};

	struct sphere_resolution {
		std::uint32_t sectors = 0;
		std::uint32_t stacks = 0;
	};

	constexpr sphere_resolution sphere_lo_resolution{ .sectors = 8, .stacks = 6 };
	constexpr sphere_resolution sphere_mid_resolution{ .sectors = 24, .stacks = 16 };
	constexpr sphere_resolution sphere_hi_resolution{ .sectors = 48, .stacks = 32 };
	constexpr std::uint32_t unit_cylinder_segments = 128;

	auto initialize(
		data& d,
		asset::data& assets
	) -> void;

	auto build_box_meshes() -> std::vector<mesh_data>;

	auto build_sphere_meshes(
		std::uint32_t sectors,
		std::uint32_t stacks
	) -> std::vector<mesh_data>;

	auto build_cylinder_meshes(
		std::uint32_t segments
	) -> std::vector<mesh_data>;
}