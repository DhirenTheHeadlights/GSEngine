export module sandbox:player_spawner;

import gse;
import std;

import :character_controller;
import :characters;
import :sidearm;

export namespace sandbox::player_spawner {
	struct [[= gse::system_state<"PlayerSpawner">{}]] data {
		std::vector<gse::id> pending;
		int spawned = 0;
		std::uint32_t generation = 0;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::world_system::spawn_player_request, gse::world_system::scene_catalog> spawn_in,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::asset::data> assets_d,
		gse::entities ents,
		gse::structural<gse::physics::transform_component>,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::physics::motor_component>,
		gse::structural<gse::physics::kinematic_target_component>,
		gse::structural<gse::physics::joint_spec>,
		gse::structural<gse::skeleton_instance_component>,
		gse::structural<gse::clip_player_component>,
		gse::structural<character_controller::component>,
		gse::structural<sidearm::component>
	) -> gse::async::task<>;
}

auto sandbox::player_spawner::run(
	gse::context& ctx,
	data& d,
	const gse::channel_read<gse::world_system::spawn_player_request, gse::world_system::scene_catalog> spawn_in,
	const gse::shared_view<gse::world_system::data> world_d,
	const gse::shared_view<gse::asset::data> assets_d,
	gse::entities,
	gse::structural<gse::physics::transform_component>,
	gse::structural<gse::physics::motion_component>,
	gse::structural<gse::physics::collision_component>,
	gse::structural<gse::physics::motor_component>,
	gse::structural<gse::physics::kinematic_target_component>,
	gse::structural<gse::physics::joint_spec>,
	gse::structural<gse::skeleton_instance_component>,
	gse::structural<gse::clip_player_component>,
	gse::structural<character_controller::component>,
	gse::structural<sidearm::component>
) -> gse::async::task<> {
	for (const auto& catalog : spawn_in.of<gse::world_system::scene_catalog>()) {
		if (catalog.generation != d.generation) {
			d.generation = catalog.generation;
			d.pending.clear();
			d.spawned = 0;
		}
	}
	for (const auto& request : spawn_in.of<gse::world_system::spawn_player_request>()) {
		d.pending.push_back(request.entity);
	}

	gse::scene* scene = world_d.active_scene_ptr;
	if (d.pending.empty() || scene == nullptr) {
		return {};
	}

	const gse::scene::mutation_scope _(*scene, ctx);
	const auto model = character_model(assets_d);
	const auto clips = character_clips(assets_d);
	const auto spacing = gse::meters(2.f);

	std::vector<gse::id> unresolved;
	for (const gse::id owner : d.pending) {
		const auto origin = gse::vec3<gse::position>(
			spacing * static_cast<float>(d.spawned),
			gse::meters(0.f),
			gse::meters(0.f)
		);

		if (const auto rig = spawn_character(*scene, owner, model, clips, origin); rig.character.exists()) {
			scene->build(rig.character)
				.with<character_controller::component>({
					.proxy = rig.proxy,
					.clips = clips,
				})
				.with<sidearm::component>({});
			++d.spawned;
		}
		else {
			unresolved.push_back(owner);
		}
	}
	d.pending = std::move(unresolved);

	return {};
}