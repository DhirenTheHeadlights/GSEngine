export module sandbox:dev_spawn_system;

import std;
import gse;

import :character_controller;
import :piston;
import :runtime_spawns;
import :sidearm;
import :tumbler;

export namespace sandbox {
	struct spawn_stress_request {};

	struct spawn_joints_request {};

	struct spawn_character_request {
		std::optional<int> count;
	};

	struct spawn_pyramid_request {};

	struct strike_pyramid_request {};

	struct spawn_lights_request {};
}

export namespace sandbox::dev_spawn {
	struct [[= gse::system_state<"DevSpawn">{}, = gse::settings::category<"Dev Spawn">{}]] data {
		[[
			= gse::settings::describe<"Sizes of the workload built by the physics stress spawn, whether it is triggered "
									  "by F5 or by the physics_stress scenario.">{},
			= gse::settings::scope<gse::settings::scope_kind::project>{}
		]]
		sandbox::stress_scene_params stress;

		[[
			= gse::settings::describe<"How many characters a spawn request builds.">{},
			= gse::settings::scope<gse::settings::scope_kind::project>{}
		]]
		sandbox::character_spawn_params characters;

		[[
			= gse::settings::describe<"Size of the pyramid built by a spawn request, whether it came from F9 or the "
									  "pyramid scenario. User-scoped so it can be swept from --engine-setting.">{}
		]]
		sandbox::pyramid_scene_params pyramid;

		[[
			= gse::settings::describe<"Where and how hard a strike request hits the pyramid. Swept from "
									  "--engine-setting so a capture shot can be reframed without a rebuild.">{}
		]]
		sandbox::pyramid_strike_params strike;

		[[
			= gse::settings::describe<"Size and brightness of the light field built by a light spawn request.">{}
		]]
		sandbox::light_field_params lights;

		gse::actions::handle spawn_stress;
		gse::actions::handle spawn_joints;
		gse::actions::handle spawn_character;
		gse::actions::handle spawn_pyramid;
		gse::actions::handle ragdoll;
		gse::id last_character;
		gse::id possessed_character;
		gse::id last_scene;
		int pending_characters = 0;
		bool character_warning_logged = false;
		bool bound = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& state,
		gse::channel_write<gse::actions::add_action_request, gse::actions::bind_axis2_request> actions_out,
		gse::channel_read<spawn_stress_request, spawn_joints_request, spawn_character_request, spawn_pyramid_request, strike_pyramid_request, spawn_lights_request> spawn_in,
		gse::channel_write<gse::animation::ragdoll_request> ragdoll_out,
		gse::channel_write<gse::physics::impulse_request> impulse_out,
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
		gse::structural<sandbox::sidearm::component>,
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
	const gse::channel_write<gse::actions::add_action_request, gse::actions::bind_axis2_request> actions_out,
	const gse::channel_read<spawn_stress_request, spawn_joints_request, spawn_character_request, spawn_pyramid_request, strike_pyramid_request, spawn_lights_request> spawn_in,
	const gse::channel_write<gse::animation::ragdoll_request> ragdoll_out,
	const gse::channel_write<gse::physics::impulse_request> impulse_out,
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
	gse::structural<sandbox::sidearm::component>,
	gse::structural<gse::skeleton_instance_component>,
	gse::structural<gse::clip_player_component>
) -> gse::async::task<> {
	if (!state.bound) {
		state.spawn_stress = gse::actions::add<"Dev_Spawn_Stress">(actions_out, gse::key::f5);
		state.spawn_joints = gse::actions::add<"Dev_Spawn_Joints">(actions_out, gse::key::f6);
		state.spawn_character = gse::actions::add<"Dev_Spawn_Character">(actions_out, gse::key::f7);
		state.ragdoll = gse::actions::add<"Dev_Ragdoll">(actions_out, gse::key::f8);
		state.spawn_pyramid = gse::actions::add<"Dev_Spawn_Pyramid">(actions_out, gse::key::f9);
		state.bound = true;
	}

	const auto& cs = gse::actions::current_state(actions_d);
	auto* scene = active_scene_ptr(world_d);

	const bool key_stress = gse::actions::pressed(state.spawn_stress, cs, actions_d);
	const bool key_joints = gse::actions::pressed(state.spawn_joints, cs, actions_d);
	const bool key_character = gse::actions::pressed(state.spawn_character, cs, actions_d);
	const bool req_stress = !spawn_in.of<spawn_stress_request>().empty();
	const bool req_joints = !spawn_in.of<spawn_joints_request>().empty();
	std::optional<int> requested_characters;
	bool req_character = false;
	for (const auto& req : spawn_in.of<spawn_character_request>()) {
		req_character = true;
		requested_characters = req.count;
	}
	const bool key_pyramid = gse::actions::pressed(state.spawn_pyramid, cs, actions_d);
	const bool req_pyramid = !spawn_in.of<spawn_pyramid_request>().empty();

	const gse::id active_scene_id = scene != nullptr ? scene->id() : gse::id{};
	const bool entered_scene = !(active_scene_id == state.last_scene);
	state.last_scene = active_scene_id;
	if (entered_scene) {
		state.possessed_character.reset();
	}
	const bool entered_pyramid_scene = entered_scene && active_scene_id == gse::find_or_generate_id("Pyramid");

	if (scene != nullptr && (key_stress || req_stress)) {
		spawn_physics_stress(*scene, state.stress);
	}
	if (scene != nullptr && (key_pyramid || req_pyramid || entered_pyramid_scene)) {
		sandbox::spawn_pyramid(*scene, state.pyramid);
	}
	if (scene != nullptr && !spawn_in.of<spawn_lights_request>().empty()) {
		spawn_light_field(*scene, state.lights);
	}
	if (scene != nullptr && !spawn_in.of<strike_pyramid_request>().empty()) {
		const auto& strike = state.strike;
		if (const float dir_len = magnitude(strike.direction); dir_len > 0.f) {
			const gse::vec3f dir = strike.direction / dir_len;
			for (int row = strike.row_begin; row < strike.row_begin + strike.rows; ++row) {
				const int count = state.pyramid.base_count - row;
				if (count <= 0) {
					continue;
				}
				const int span = std::max(1, static_cast<int>(static_cast<float>(count) * strike.width_fraction));
				for (int col = 0; col < span && col < count; ++col) {
					if (const auto block = gse::try_find(std::format("PyramidBlock_{}_{}", row, col))) {
						impulse_out.push<gse::physics::impulse_request>({
							.target = *block,
							.impulse = gse::vec3<gse::impulse>(
								dir.x() * strike.strength,
								dir.y() * strike.strength,
								dir.z() * strike.strength
							),
						});
					}
				}
			}
		}
	}
	if (scene != nullptr && (key_joints || req_joints)) {
		spawn_joint_test(*scene);
	}
	if (key_character || req_character) {
		state.pending_characters = requested_characters.value_or(state.characters.count);
	}
	if (scene != nullptr && state.pending_characters > 0) {
		constexpr float character_spacing = 2.f;
		const auto rig = gse::asset::get<gse::skinned_model>(assets_d, "SkinnedModels/x_bot.v3");
		const gse::animation::locomotion_blend clips{
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
		};

		gse::id spawned;
		for (int i = 0; i < state.pending_characters; ++i) {
			const bool possess = !state.possessed_character.exists();
			spawned = sandbox::spawn_character(
				*scene,
				i,
				possess,
				rig,
				clips,
				gse::vec3<gse::position>(static_cast<float>(i) * character_spacing, 0.f, 0.f)
			);
			if (!spawned.exists()) {
				break;
			}
			state.last_character = spawned;
			if (possess) {
				state.possessed_character = spawned;
			}
		}

		if (spawned.exists()) {
			state.pending_characters = 0;
		}
		else if (!state.character_warning_logged) {
			state.character_warning_logged = true;
			gse::log::println(
				gse::log::level::warning,
				"dev_spawn: character spawn produced nothing, so its skinned model or clips are not resolved yet; retrying each tick"
			);
		}
	}
	if (state.last_character.exists() && gse::actions::pressed(state.ragdoll, cs, actions_d)) {
		ragdoll_out.push<gse::animation::ragdoll_request>({
			.character = state.last_character,
		});
	}

	return {};
}