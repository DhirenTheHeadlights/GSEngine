export module gs:dev_spawn_system;

import std;
import gse;

import :piston;
import :runtime_spawns;
import :tumbler;

export namespace gs {
	struct spawn_stress_request {};

	struct spawn_joints_request {};
}

export namespace gs::dev_spawn {
	struct [[= gse::system_state<"DevSpawn">{}]] data {
		gse::actions::handle spawn_stress;
		gse::actions::handle spawn_joints;
		bool bound = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& state,
		gse::shared_view<gse::actions::data> actions_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::structural<gse::physics::transform_component>,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::primitive_box_spec>,
		gse::structural<gse::primitive_sphere_spec>,
		gse::structural<gse::physics::joint_spec>,
		gse::structural<gse::physics::muscle_component>,
		gse::structural<gse::physics::joint_drive_component>,
		gse::structural<gs::tumbler::component>,
		gse::structural<gs::piston::component>
	) -> gse::async::task<>;
}

namespace gs {
	auto active_scene_ptr(
		gse::shared_view<gse::world_system::data> w
	) -> gse::scene*;
}

auto gs::active_scene_ptr(const gse::shared_view<gse::world_system::data> w) -> gse::scene* {
	if (!w.active_scene) {
		return nullptr;
	}
	const auto it = w.scenes.find(*w.active_scene);
	if (it == w.scenes.end()) {
		return nullptr;
	}
	return it->second.get();
}

auto gs::dev_spawn::run(
	gse::context& ctx,
	data& state,
	const gse::shared_view<gse::actions::data> actions_d,
	const gse::shared_view<gse::world_system::data> world_d,
	gse::structural<gse::physics::transform_component>,
	gse::structural<gse::physics::motion_component>,
	gse::structural<gse::physics::collision_component>,
	gse::structural<gse::primitive_box_spec>,
	gse::structural<gse::primitive_sphere_spec>,
	gse::structural<gse::physics::joint_spec>,
	gse::structural<gse::physics::muscle_component>,
	gse::structural<gse::physics::joint_drive_component>,
	gse::structural<gs::tumbler::component>,
	gse::structural<gs::piston::component>
) -> gse::async::task<> {
	if (!state.bound) {
		state.spawn_stress = gse::actions::add<"Dev_Spawn_Stress">(ctx.channels, gse::key::f5);
		state.spawn_joints = gse::actions::add<"Dev_Spawn_Joints">(ctx.channels, gse::key::f6);
		state.bound = true;
	}

	const auto& cs = gse::actions::current_state(actions_d);
	auto* scene = active_scene_ptr(world_d);

	const bool key_stress = gse::actions::pressed(state.spawn_stress, cs, actions_d);
	const bool key_joints = gse::actions::pressed(state.spawn_joints, cs, actions_d);
	const bool req_stress = !ctx.read_channel<spawn_stress_request>().empty();
	const bool req_joints = !ctx.read_channel<spawn_joints_request>().empty();

	if (scene != nullptr && (key_stress || req_stress)) {
		spawn_physics_stress(*scene);
	}
	if (scene != nullptr && (key_joints || req_joints)) {
		spawn_joint_test(*scene);
	}

	return {};
}
