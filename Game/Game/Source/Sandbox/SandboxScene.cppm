export module gs:sandbox_scene;

import std;
import gse;

import :entity_builders;
import :orbit_camera;
import :player;

export namespace gs {
	auto sandbox_scene_setup(gse::scene& s) -> void;
}

auto gs::sandbox_scene_setup(gse::scene& s) -> void {
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
		sc.registry().add_component<gse::free_camera::component>(
			player_id,
			{
				.initial_position = gse::vec3<gse::position>(0.f, 2.f, 0.f),
			}
		);
		return player_id;
	});

	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn("Floor", gs::static_collider(gse::vec3<gse::position>(0.f, -0.5f, 0.f), floor_size));

	s.build("Sun").with<gse::directional_light_component>({
		.color = gse::vec3f(1.0f, 0.97f, 0.92f),
		.intensity = 1.6f,
		.direction = gse::vec3f(0.25f, -1.0f, 0.15f),
		.ambient_strength = 0.18f,
	});

	const auto player_id = s.build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 2.f, 0.f),
		})
		.identify();

	s.build("Orbit Camera").with<gs::orbit_camera::component>({
		.target = player_id,
	});

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 5.f, 10.f),
		});
}
