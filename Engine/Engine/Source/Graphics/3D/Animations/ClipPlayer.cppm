export module gse.graphics:clip_player;

import std;

import :animation_components;
import :blend_space;
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

	struct clip_binding {
		static constexpr std::uint16_t no_track = std::numeric_limits<std::uint16_t>::max();

		std::array<std::uint16_t, skeleton_instance_component::max_bones> track_by_slot{};
		velocity ground_speed;
	};

	struct clip_binding_key {
		id model;
		std::uint32_t model_version = 0;
		id clip;
		std::uint32_t clip_version = 0;

		auto operator<=>(
			const clip_binding_key&
		) const = default;
	};

	struct [[= system_state<"ClipPlayer">{}, = settings::category<"Animation">{}]] data {
		[[= settings::describe<"Advance clip playback each frame.">{}]] bool play = true;

		std::flat_map<clip_binding_key, clip_binding> bindings;
	};

	auto compose(
		const joint_transform& parent,
		const vec3<displacement>& offset,
		const quat& rotation
	) -> joint_transform;

	auto binding_for(
		data& d,
		const skinned_model& model,
		const clip_asset& clip,
		clip_binding_key key
	) -> const clip_binding&;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<physics::interpolation_state> interp_in,
		write<clip_player_component> players,
		read<skeleton_instance_component> skeletons,
		read<physics::transform_component> transforms,
		read<physics::motion_component> motions,
		write<physics::kinematic_target_component> targets
	) -> async::task<>;
}