export module gs:arena;

import std;
import gse;

import :entity_builders;

export namespace gs::arena {
	auto create(
		gse::asset::state& assets,
		gse::scene* scene
	) -> void;
}

auto gs::arena::create(gse::asset::state& assets, gse::scene* scene) -> void {
	constexpr auto arena_position = gse::vec3<gse::position>(0.f, 0.f, 0.f);

	constexpr gse::length arena_size = gse::meters(100.f);
	constexpr gse::length wall_thickness = gse::meters(10.f);

	build_static_box(
		assets,
		scene,
		"Front Wall",
		arena_position + gse::vec3<gse::length>(0.f, 0.f, arena_size / 2.f),
		gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
	);

	build_static_box(
		assets,
		scene,
		"Top Wall",
		arena_position + gse::vec3<gse::length>(0.f, arena_size / 2.f, 0.f),
		gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
	);

	build_static_box(
		assets,
		scene,
		"Bottom Wall",
		arena_position + gse::vec3<gse::length>(0.f, -arena_size / 2.f, 0.f),
		gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
	);

	build_static_box(
		assets,
		scene,
		"Left Wall",
		arena_position + gse::vec3<gse::length>(-arena_size / 2.f, 0.f, 0.f),
		gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
	);

	build_static_box(
		assets,
		scene,
		"Right Wall",
		arena_position + gse::vec3<gse::length>(arena_size / 2.f, 0.f, 0.f),
		gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
	);

	build_static_box(
		assets,
		scene,
		"Back Wall",
		arena_position + gse::vec3<gse::length>(0.f, 0.f, -arena_size / 2.f),
		gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
	);
}
