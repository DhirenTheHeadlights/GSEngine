export module sandbox:player_spawner;

import std;
import gse;

import :runtime_spawns;

export namespace sandbox::player_spawner {
	struct [[= gse::system_state<"PlayerSpawner">{}]] data {
		std::vector<gse::id> pending;
		gse::scene* scene = nullptr;
		int spawned = 0;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::world_system::spawn_player_request, gse::world_system::scene_catalog> spawn_in,
		gse::shared_view<gse::asset::data> assets_d,
		gse::entities ents,
		gse::structural<gse::physics::transform_component>,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::physics::motor_component>,
		gse::structural<gse::physics::kinematic_target_component>,
		gse::structural<gse::physics::joint_spec>,
		gse::structural<gse::skeleton_instance_component>,
		gse::structural<gse::clip_player_component>
	) -> gse::async::task<>;
}

auto sandbox::player_spawner::run(
	gse::context& ctx,
	data& d,
	const gse::channel_read<gse::world_system::spawn_player_request, gse::world_system::scene_catalog> spawn_in,
	const gse::shared_view<gse::asset::data> assets_d,
	gse::entities,
	gse::structural<gse::physics::transform_component>,
	gse::structural<gse::physics::motion_component>,
	gse::structural<gse::physics::collision_component>,
	gse::structural<gse::physics::motor_component>,
	gse::structural<gse::physics::kinematic_target_component>,
	gse::structural<gse::physics::joint_spec>,
	gse::structural<gse::skeleton_instance_component>,
	gse::structural<gse::clip_player_component>
) -> gse::async::task<> {
	for (const auto& catalog : spawn_in.of<gse::world_system::scene_catalog>()) {
		d.scene = catalog.active_scene;
	}
	for (const auto& request : spawn_in.of<gse::world_system::spawn_player_request>()) {
		d.pending.push_back(request.entity);
	}

	if (d.pending.empty() || d.scene == nullptr) {
		return {};
	}

	const gse::scene::mutation_scope scope(*d.scene, ctx);
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

		if (spawn_character(*d.scene, owner, model, clips, origin).character.exists()) {
			++d.spawned;
		}
		else {
			unresolved.push_back(owner);
		}
	}
	d.pending = std::move(unresolved);

	return {};
}
