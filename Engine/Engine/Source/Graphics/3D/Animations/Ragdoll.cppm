export module gse.graphics:ragdoll;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.physics;
import std;

import :animation_components;

export namespace gse::animation {
	struct ragdoll_request {
		id character;
	};

	struct ragdoll_anchor {
		quat root = quat(1.f, 0.f, 0.f, 0.f);
		quat proxy = quat(1.f, 0.f, 0.f, 0.f);
	};

	struct [[= system_state<"Ragdoll">{}]] data {
		std::flat_map<id, ragdoll_anchor> anchors;
	};

	[[= system_run<>{}]]
	auto ragdoll(
		context& ctx,
		data& d,
		channel_read<ragdoll_request> ragdoll_in,
		write<skeleton_instance_component> skeletons,
		write<clip_player_component> players,
		channel_write<
			physics::set_request<^^physics::transform_component::position>,
			physics::set_request<^^physics::transform_component::orientation>,
			physics::set_request<^^physics::motion_component::current_velocity>,
			physics::set_request<^^physics::motion_component::angular_velocity>,
			physics::set_request<^^physics::motion_component::body>,
			physics::set_request<^^physics::collision_component::resolve_collisions>
		> physics_out,
		read<physics::motion_component> motions,
		read<physics::collision_component> collisions,
		read<physics::transform_component> transforms
	) -> async::task<>;
}