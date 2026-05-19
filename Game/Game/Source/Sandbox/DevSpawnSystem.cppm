export module gs:dev_spawn_system;

import std;
import gse;

import :runtime_spawns;

export namespace gs {
	struct[[= gse::same_frame_channel]] spawn_stress_request {};

	struct[[= gse::same_frame_channel]] spawn_joints_request {};

	struct dev_spawn_system {
		struct data {
			gse::actions::handle spawn_stress;
			gse::actions::handle spawn_joints;
			bool bound = false;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::actions::system::data& actions_d,
			const gse::world_system::data& world_d
		) -> gse::async::task<>;
	};
}

namespace gs {
	auto active_scene_ptr(const gse::world_system::data& w) -> gse::scene*;
}

auto gs::active_scene_ptr(const gse::world_system::data& w) -> gse::scene* {
	if (!w.active_scene) {
		return nullptr;
	}
	const auto it = w.scenes.find(*w.active_scene);
	if (it == w.scenes.end()) {
		return nullptr;
	}
	return it->second.get();
}

auto gs::dev_spawn_system::run(
	gse::run_context& ctx,
	data& d,
	const gse::actions::system::data& actions_d,
	const gse::world_system::data& world_d
) -> gse::async::task<> {
	while (true) {
		if (!d.bound) {
			d.spawn_stress = gse::actions::add<"Dev_Spawn_Stress">(ctx.channels, gse::key::f5);
			d.spawn_joints = gse::actions::add<"Dev_Spawn_Joints">(ctx.channels, gse::key::f6);
			d.bound = true;
		}

		const auto& cs = gse::actions::system::current_state(actions_d);
		auto* scene = active_scene_ptr(world_d);

		const bool key_stress = gse::actions::pressed(d.spawn_stress, cs, actions_d);
		const bool key_joints = gse::actions::pressed(d.spawn_joints, cs, actions_d);
		const bool req_stress = !ctx.read_channel<spawn_stress_request>().empty();
		const bool req_joints = !ctx.read_channel<spawn_joints_request>().empty();

		if (scene != nullptr && (key_stress || req_stress)) {
			spawn_physics_stress(*scene);
		}
		if (scene != nullptr && (key_joints || req_joints)) {
			spawn_joint_test(*scene);
		}

		co_await ctx.next_tick();
	}
}
