export module gs:main_test_scene;

import :player;
import :arena;
import :entity_builders;

import std;
import gse;

export namespace gs {
	class main_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override;
	};
}

auto gs::main_test_scene::initialize() -> void {
	m_owner->set_player_factory([next_id = 0u](gse::scene& s, std::optional<gse::id> server_id) mutable -> gse::id {
		gse::id player_id;
		if (server_id) {
			player_id = gse::find_or_generate_id(server_id->number());
			auto& reg = s.registry();
			reg.ensure_exists(player_id);
			reg.ensure_active(player_id);
		}
		else {
			player_id = s.add_entity(std::format("Player_{}", next_id++));
		}
		s.registry().add_component<gs::player::component>(player_id, gs::player::component_data{
			.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f),
		});
		return player_id;
	});

	arena::create(this);

	build_box(this, "Smaller Box",
		gse::vec3<gse::position>(2.f, -40.f, 2.f),
		gse::vec3<gse::length>(2.f, 2.f, 2.f));

	build_box(this, "Bigger Box",
		gse::vec3<gse::position>(-2.f, -40.f, 2.f),
		gse::vec3<gse::length>(4.f, 4.f, 4.f),
		gse::kilograms(100000.f));

	build_sphere_light(this, "Center Sphere Light",
		gse::vec3<gse::position>(0.f, -30.f, 0.f),
		gse::meters(1.f));

	build_sphere(this, "Second Sphere",
		gse::vec3<gse::position>(0.f, 0.f, 20.f),
		gse::meters(1.f),
		18,
		10);
}
