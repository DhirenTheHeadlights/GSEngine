export module gs:sandbox_scene;

import std;
import gse;

import :balance;
import :entity_builders;
import :humanoid_skeleton;
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

	const auto player_initial_pos = gse::vec3<gse::position>(0.f, 1.05f, 0.f);
	const auto player_initial_yaw = gse::degrees(-90.f);
	const auto player_initial_orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), player_initial_yaw);
	const auto humanoid_handle = gs::spawn_humanoid(s, player_initial_pos, player_initial_orientation);
	const auto pelvis_id = humanoid_handle.bone_ids[0];

	if (auto* m = s.registry().try_component<gse::physics::motion_component>(pelvis_id)) {
		if (auto* dyn = std::get_if<gse::physics::dynamic_body>(&m->body)) {
			dyn->update_orientation = false;
		}
	}

	s.registry().add_component<gse::physics::motor_component>(
		pelvis_id,
		{
			.requires_ground_contact = false,
			.max_force = gse::newtons(600.f),
		}
	);

	s.registry().add_component<gs::balance::component>(
		pelvis_id,
		{
			.support_a = humanoid_handle.bone_ids[11],
			.support_b = humanoid_handle.bone_ids[14],
			.response_time = gse::seconds(0.5f),
			.damping = 3.0f,
			.max_correction = gse::meters_per_second(1.2f),
		}
	);

	const auto player_id = s.build("Player")
		.with<gs::player::component>({
			.initial_position = player_initial_pos,
			.pelvis_id = pelvis_id,
			.hip_l_id = humanoid_handle.joint_ids[8],
			.knee_l_id = humanoid_handle.joint_ids[9],
			.hip_r_id = humanoid_handle.joint_ids[11],
			.knee_r_id = humanoid_handle.joint_ids[12],
			.foot_l_id = humanoid_handle.bone_ids[11],
			.foot_r_id = humanoid_handle.bone_ids[14],
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
