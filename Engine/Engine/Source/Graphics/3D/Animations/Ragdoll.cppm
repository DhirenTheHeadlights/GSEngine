export module gse.graphics:ragdoll;

import std;

import :animation_components;
import :skinned_model;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.assets;
import gse.physics;

export namespace gse::animation {
	struct ragdoll_request {
		id character;
	};

	struct ragdoll_anchor {
		quat root = quat(1.f, 0.f, 0.f, 0.f);
		quat proxy = quat(1.f, 0.f, 0.f, 0.f);
	};

	struct [[= gse::system_state<"Ragdoll">{}]] data {
		std::flat_map<id, ragdoll_anchor> anchors;
	};

	[[= gse::system_run<>{}]]
	auto ragdoll(
		context& ctx,
		data& d,
		write<skeleton_instance_component> skeletons,
		write<clip_player_component> players,
		write<physics::motion_component> motions,
		write<physics::collision_component> collisions,
		write<physics::transform_component> transforms
	) -> async::task<>;
}
