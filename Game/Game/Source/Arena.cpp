#include "Arena.h"

#include "Core/ObjectRegistry.h"

struct wall_hook final : gse::hook<gse::object> {
	using hook::hook;

	auto initialize() -> void override {
		gse::registry::get_component<gse::physics::collision_component>(owner_id).resolve_collisions = false;
		gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
	}
};

namespace {
	auto create_arena_wall(const gse::vec3<gse::units::meters>& position, const gse::vec3<gse::units::meters>& size) -> gse::object* {
		gse::object* wall = create_box(position, size);
		gse::registry::add_object_hook(wall->index, wall_hook());
		return wall;
	}
}

auto game::arena::create(gse::scene* scene) -> void {
	const auto arena_position = gse::vec3<gse::units::meters>(0.f, 0.f, 0.f);

	const gse::length arena_size = gse::meters(1000.f);
	const gse::length wall_thickness = gse::meters(1.f);

	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(0.f, 0.f, arena_size.as<gse::units::meters>() / 2.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>(), wall_thickness)), "Front Wall");
	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(0.f, arena_size.as<gse::units::meters>() / 2.f, 0.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), wall_thickness, arena_size.as<gse::units::meters>())), "Top Wall");
	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(0.f, -arena_size.as<gse::units::meters>() / 2.f, 0.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), wall_thickness, arena_size.as<gse::units::meters>())), "Bottom Wall");
	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(-arena_size.as<gse::units::meters>() / 2.f, 0.f, 0.f), gse::vec3<gse::units::meters>(wall_thickness, arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>())), "Left Wall");
	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>() / 2.f, 0.f, 0.f), gse::vec3<gse::units::meters>(wall_thickness, arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>())), "Right Wall");
	scene->add_object(create_arena_wall(arena_position + gse::vec3<gse::units::meters>(0.f, 0.f, -arena_size.as<gse::units::meters>() / 2.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>(), wall_thickness)), "Back Wall");
}
