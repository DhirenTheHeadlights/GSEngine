module game.arena;

import std;
import gse;

struct wall_hook final : gse::hook<gse::entity> {
	using hook::hook;

	auto initialize() -> void override {
		gse::registry::get_component<gse::physics::collision_component>(owner_id).resolve_collisions = false;
		gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
	}
};

auto create_arena_wall(const gse::vec3<gse::length>& position, const gse::vec3<gse::length>& size) -> std::uint32_t {
	const std::uint32_t wall_uuid = gse::create_box(position, size);
	gse::registry::add_entity_hook(wall_uuid, std::make_unique<wall_hook>());
	return wall_uuid;
}

auto game::arena::create(gse::scene* scene) -> void {
	constexpr auto arena_position = gse::vec3<gse::length>(0.f, 0.f, 0.f);

	constexpr gse::length arena_size = gse::meters(1000.f);
	constexpr gse::length wall_thickness = gse::meters(1.f);

	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(0.f, 0.f, arena_size / 2.f), gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)), "Front Wall");
	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(0.f, arena_size / 2.f, 0.f), gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)), "Top Wall");
	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(0.f, -arena_size / 2.f, 0.f), gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)), "Bottom Wall");
	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(-arena_size / 2.f, 0.f, 0.f), gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)), "Left Wall");
	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(arena_size / 2.f, 0.f, 0.f), gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)), "Right Wall");
	scene->add_entity(create_arena_wall(arena_position + gse::vec3<gse::length>(0.f, 0.f, -arena_size / 2.f), gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)), "Back Wall");
}