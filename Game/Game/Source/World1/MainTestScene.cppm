export module gs:main_test_scene;

import std;
import gse;

import :player;
import :arena;
import :entity_builders;

export namespace gs {
	auto main_test_scene_setup(
		gse::scene& s
	) -> void;
}

auto gs::main_test_scene_setup(gse::scene& s) -> void {
	s.set_player_factory([next_id = 0u](gse::scene& sc, std::optional<gse::id> server_id) mutable -> gse::id {
		gse::id player_id;
		if (server_id) {
			player_id = gse::find_or_generate_id(server_id->number());
			auto& reg = sc.registry();
			reg.ensure_exists(player_id);
			reg.ensure_active(player_id);
		}
		else {
			player_id = sc.add_entity(std::format("Player_{}", next_id++));
		}
		sc.registry().add_component<gse::free_camera::component>(player_id, {
																				.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f),
																			});
		return player_id;
	});

	arena::create(&s);

	s.spawn("Smaller Box", gs::box(gse::vec3<gse::position>(2.f, -40.f, 2.f), gse::vec3<gse::length>(2.f, 2.f, 2.f)));

	s.spawn("Bigger Box", gs::box(gse::vec3<gse::position>(-2.f, -40.f, 2.f), gse::vec3<gse::length>(4.f, 4.f, 4.f), gse::kilograms(100000.f)));

	s.spawn("Center Sphere Light", gs::sphere_light(gse::vec3<gse::position>(0.f, -30.f, 0.f), gse::meters(1.f)));

	s.spawn("Second Sphere", gs::sphere(gse::vec3<gse::position>(0.f, 0.f, 20.f), gse::meters(1.f), gse::sphere_lod::lo));
}
