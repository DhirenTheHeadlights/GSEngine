export module gs:sandbox_scene;

import std;
import gse;

import :entity_builders;
import :humanoid_skeleton;
import :locomotion_types;
import :orbit_camera;
import :player;

export namespace gs {
	auto sandbox_scene_setup(
		gse::scene& s
	) -> void;
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
	s.spawn(
		"Floor",
		gs::static_box(
			gse::vec3<gse::position>(0.f, -0.501f, 0.f),
			floor_size,
			gse::quat(1.f, 0.f, 0.f, 0.f),
			gse::vec3f(0.08f, 0.08f, 0.09f),
			0.45f,
			0.0f
		)
	);

	const auto player_initial_pos = gse::vec3<gse::position>(0.f, 1.005f, 0.f);
	const auto player_initial_yaw = gse::degrees(-90.f);
	const auto player_initial_orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), player_initial_yaw);
	const auto humanoid_handle = gs::spawn_humanoid(s, player_initial_pos, player_initial_orientation);
	const auto pelvis_id = humanoid_handle.bone_ids[0];

	if (auto* m = s.registry().try_component<gse::physics::motion_component>(pelvis_id)) {
		if (auto* dyn = std::get_if<gse::physics::dynamic_body>(&m->body)) {
			dyn->update_orientation = true;
		}
	}

	s.registry().add_component<gs::locomotion::skeleton_refs>(
		pelvis_id,
		{
			.pelvis_id = pelvis_id,
			.thigh_l_id = humanoid_handle.bone_ids[9],
			.shin_l_id = humanoid_handle.bone_ids[10],
			.foot_l_id = humanoid_handle.bone_ids[11],
			.thigh_r_id = humanoid_handle.bone_ids[12],
			.shin_r_id = humanoid_handle.bone_ids[13],
			.foot_r_id = humanoid_handle.bone_ids[14],
			.hip_l_joint_id = humanoid_handle.joint_ids[8],
			.knee_l_joint_id = humanoid_handle.joint_ids[9],
			.hip_r_joint_id = humanoid_handle.joint_ids[11],
			.knee_r_joint_id = humanoid_handle.joint_ids[12],
			.thigh_length = gse::meters(0.45f),
			.shin_length = gse::meters(0.40f),
			.hip_offset_lateral = gse::meters(0.075f),
			.hip_offset_below_pelvis = gse::meters(0.10f),
			.pelvis_target_height = gse::meters(0.95f),
		}
	);

	s.registry().add_component<gs::locomotion::intent>(pelvis_id, {});
	s.registry().add_component<gs::locomotion::state>(pelvis_id, {});
	s.registry().add_component<gs::locomotion::gait>(pelvis_id, {});
	s.registry().add_component<gs::locomotion::plan>(pelvis_id, {});
	s.registry().add_component<gs::locomotion::leg_context>(pelvis_id, {});
	s.registry().add_component<gse::physics::motor_component>(
		pelvis_id,
		{
			.velocity_drive_target = {},
			.horizontal_only = true,
			.requires_ground_contact = false,
			.max_force = gse::newtons(180.f),
		}
	);

	const auto player_id = s.build("Player")
		.with<gs::player::component>({
			.pelvis_id = pelvis_id,
			.yaw = player_initial_yaw,
		})
		.identify();

	s.build("Orbit Camera")
		.with<gs::orbit_camera::component>({
			.target = player_id,
		});

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 5.f, 10.f),
		});
}
