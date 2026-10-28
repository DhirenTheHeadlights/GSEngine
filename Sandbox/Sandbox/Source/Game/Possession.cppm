export module sandbox:possession;

import gse;

import :character_controller;
import :characters;
import :orbit_camera;
import :role;
import :sidearm;

export namespace sandbox::player {
	struct [[= gse::system_state<"Player">{}]]
	data {
		gse::id pending_possession;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::world_system::possess_player_request> possess_in,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::asset::data> assets_d,
		gse::read<gse::skeleton_instance_component> skeletons,
		gse::entities ents,
		gse::structural<character_controller::component>,
		gse::structural<orbit_camera::component>,
		gse::structural<sidearm::component>,
		gse::structural<gse::free_camera::component> cameras
	) -> gse::async::task<>;
}

auto sandbox::player::run(gse::context& ctx, data& d, const gse::channel_read<gse::world_system::possess_player_request> possess_in, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::asset::data> assets_d, gse::read<gse::skeleton_instance_component> skeletons, gse::entities, gse::structural<character_controller::component>, gse::structural<orbit_camera::component>, gse::structural<sidearm::component>, gse::structural<gse::free_camera::component> cameras) -> gse::async::task<> {
	if (!is_client(world_d)) {
		for (const auto& request : possess_in.of<gse::world_system::possess_player_request>()) {
			cameras.add(
				request.entity,
				{
					.initial_position = gse::vec3<gse::position>(gse::meters(0.f, 2.f, 0.f)),
				}
			);
		}
		return {};
	}

	for (const auto& request : possess_in.of<gse::world_system::possess_player_request>()) {
		d.pending_possession = request.entity;
	}
	if (d.pending_possession != world_d.local_controlled_entity) {
		d.pending_possession = {};
	}

	auto* scene = world_d.active_scene_ptr;
	if (!d.pending_possession.exists() || scene == nullptr) {
		return {};
	}

	const auto* skeleton = skeletons.find(d.pending_possession);
	if (skeleton == nullptr) {
		return {};
	}

	const gse::scene::mutation_scope _(*scene, ctx);
	possess_character(
		*scene,
		{
			.character = d.pending_possession,
			.proxy = skeleton->proxy,
		},
		character_clips(assets_d)
	);
	d.pending_possession = {};

	return {};
}
