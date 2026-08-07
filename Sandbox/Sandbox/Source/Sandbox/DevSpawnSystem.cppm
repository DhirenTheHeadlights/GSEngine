export module sandbox:dev_spawn_system;

import std;
import gse;

import :character_controller;
import :piston;
import :runtime_spawns;
import :tumbler;

export namespace sandbox {
	struct spawn_stress_request {};

	struct spawn_joints_request {};
}

export namespace sandbox::dev_spawn {
	struct [[= gse::system_state<"DevSpawn">{}, = gse::settings::category<"Dev Spawn">{}]] data {
		[[
			= gse::settings::describe<"Sizes of the workload built by the physics stress spawn, whether it is triggered "
									  "by F5 or by the physics_stress scenario.">{},
			= gse::settings::scope<gse::settings::scope_kind::project>{}
		]]
		sandbox::stress_scene_params stress;

		gse::actions::handle spawn_stress;
		gse::actions::handle spawn_joints;
		gse::actions::handle spawn_character;
		gse::actions::handle ragdoll;
		gse::id last_character;
		bool bound = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& state,
		gse::shared_view<gse::actions::data> actions_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::asset::data> assets_d,
		gse::structural<gse::physics::transform_component>,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::primitive_box_spec>,
		gse::structural<gse::primitive_sphere_spec>,
		gse::structural<gse::physics::joint_spec>,
		gse::structural<gse::physics::muscle_component>,
		gse::structural<gse::physics::joint_drive_component>,
		gse::structural<sandbox::tumbler::component>,
		gse::structural<sandbox::piston::component>,
		gse::structural<gse::physics::kinematic_target_component>,
		gse::structural<gse::physics::motor_component>,
		gse::structural<sandbox::character_controller::component>,
		gse::structural<sandbox::orbit_camera::component>,
		gse::structural<gse::skeleton_instance_component>,
		gse::structural<gse::clip_player_component>
	) -> gse::async::task<>;
}

namespace sandbox {
	auto active_scene_ptr(
		gse::shared_view<gse::world_system::data> w
	) -> gse::scene*;
}

auto sandbox::active_scene_ptr(const gse::shared_view<gse::world_system::data> w) -> gse::scene* {
	return w.active_scene_ptr;
}

auto sandbox::dev_spawn::run(
	gse::context& ctx,
	data& state,
	const gse::shared_view<gse::actions::data> actions_d,
	const gse::shared_view<gse::world_system::data> world_d,
	const gse::shared_view<gse::asset::data> assets_d,
	gse::structural<gse::physics::transform_component>,
	gse::structural<gse::physics::motion_component>,
	gse::structural<gse::physics::collision_component>,
	gse::structural<gse::primitive_box_spec>,
	gse::structural<gse::primitive_sphere_spec>,
	gse::structural<gse::physics::joint_spec>,
	gse::structural<gse::physics::muscle_component>,
	gse::structural<gse::physics::joint_drive_component>,
	gse::structural<sandbox::tumbler::component>,
	gse::structural<sandbox::piston::component>,
	gse::structural<gse::physics::kinematic_target_component>,
	gse::structural<gse::physics::motor_component>,
	gse::structural<sandbox::character_controller::component>,
	gse::structural<sandbox::orbit_camera::component>,
	gse::structural<gse::skeleton_instance_component>,
	gse::structural<gse::clip_player_component>
) -> gse::async::task<> {
	if (!state.bound) {
		state.spawn_stress = gse::actions::add<"Dev_Spawn_Stress">(ctx.channels, gse::key::f5);
		state.spawn_joints = gse::actions::add<"Dev_Spawn_Joints">(ctx.channels, gse::key::f6);
		state.spawn_character = gse::actions::add<"Dev_Spawn_Character">(ctx.channels, gse::key::f7);
		state.ragdoll = gse::actions::add<"Dev_Ragdoll">(ctx.channels, gse::key::f8);
		state.bound = true;
	}

	const auto& cs = gse::actions::current_state(actions_d);
	auto* scene = active_scene_ptr(world_d);

	const bool key_stress = gse::actions::pressed(state.spawn_stress, cs, actions_d);
	const bool key_joints = gse::actions::pressed(state.spawn_joints, cs, actions_d);
	const bool req_stress = !ctx.read_channel<spawn_stress_request>().empty();
	const bool req_joints = !ctx.read_channel<spawn_joints_request>().empty();

	if (scene != nullptr && (key_stress || req_stress)) {
		spawn_physics_stress(*scene, state.stress);
	}
	if (scene != nullptr && (key_joints || req_joints)) {
		spawn_joint_test(*scene);
	}
	if (scene != nullptr && gse::actions::pressed(state.spawn_character, cs, actions_d)) {
		state.last_character = sandbox::spawn_character(
			*scene,
			gse::asset::get<gse::skinned_model>(assets_d, "SkinnedModels/x_bot.v3"),
			gse::animation::locomotion_blend{
				.idle = gse::asset::get<gse::clip_asset>(assets_d, "Clips/idle"),
				.walk = {
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe_walking"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe_walking"),
				},
				.run = {
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/running"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe"),
					gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe"),
				},
			},
			gse::vec3<gse::position>(0.f, 0.f, 0.f)
		);
	}
	if (state.last_character.exists() && gse::actions::pressed(state.ragdoll, cs, actions_d)) {
		ctx.channels.push<gse::animation::ragdoll_request>({
			.character = state.last_character,
		});
	}

	return {};
}