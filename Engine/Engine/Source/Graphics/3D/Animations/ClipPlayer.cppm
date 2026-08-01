export module gse.graphics:clip_player;

import std;

import :animation_components;
import :clip;
import :skinned_model;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.time;
import gse.assets;
import gse.physics;

export namespace gse::animation {
	struct joint_transform {
		vec3<current_position> position;
		quat orientation = quat(1.f, 0.f, 0.f, 0.f);
	};

	struct [[= gse::system_state<"ClipPlayer">{}, = gse::settings::category<"Animation">{}]] data {
		[[= gse::settings::describe<"Advance clip playback each frame.">{}]] bool play = true;
	};

	auto compose(
		const joint_transform& parent,
		const vec3<displacement>& offset,
		const quat& rotation
	) -> joint_transform;

	auto advance(
		clip_player_component& player,
		const clip_asset& clip,
		time dt
	) -> void;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		write<clip_player_component> players,
		read<skeleton_instance_component> skeletons,
		read<physics::transform_component> transforms,
		write<physics::kinematic_target_component> targets
	) -> async::task<>;
}
