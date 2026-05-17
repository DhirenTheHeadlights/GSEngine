export module gs:arena;

import std;
import gse;

import :entity_builders;

export namespace gs::arena {
	auto create(
		gse::scene* scene
	) -> void;
}

auto gs::arena::create(gse::scene* scene) -> void {
	constexpr auto arena_position = gse::vec3<gse::position>(0.f, 0.f, 0.f);

	constexpr gse::length arena_size = gse::meters(100.f);
	constexpr gse::length wall_thickness = gse::meters(10.f);

	scene->spawn("Front Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(0.f, 0.f, arena_size / 2.f),
		gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
	));

	scene->spawn("Top Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(0.f, arena_size / 2.f, 0.f),
		gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
	));

	scene->spawn("Bottom Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(0.f, -arena_size / 2.f, 0.f),
		gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
	));

	scene->spawn("Left Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(-arena_size / 2.f, 0.f, 0.f),
		gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
	));

	scene->spawn("Right Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(arena_size / 2.f, 0.f, 0.f),
		gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
	));

	scene->spawn("Back Wall", gs::static_box(
		arena_position + gse::vec3<gse::length>(0.f, 0.f, -arena_size / 2.f),
		gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
	));
}
