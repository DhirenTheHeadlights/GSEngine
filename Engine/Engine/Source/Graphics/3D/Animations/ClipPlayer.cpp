module gse.graphics:clip_player_impl;

import std;

import :clip_player;
import :animation_components;
import :clip;
import :skinned_model;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.time;
import gse.assets;
import gse.physics;

auto gse::animation::compose(const joint_transform& parent, const vec3<displacement>& offset, const quat& rotation) -> joint_transform {
	return {
		.position = parent.position + rotate_vector(parent.orientation, offset),
		.orientation = normalize(parent.orientation * rotation),
	};
}

auto gse::animation::advance(clip_player_component& player, const clip_asset& clip, const time dt) -> void {
	if (clip.length() <= time{}) {
		return;
	}

	player.elapsed += dt * player.speed;

	if (!clip.loops()) {
		player.elapsed = std::min(player.elapsed, clip.length());
		return;
	}

	while (player.elapsed >= clip.length()) {
		player.elapsed -= clip.length();
	}
}

auto gse::animation::run(context& ctx, data& d, write<clip_player_component> players, read<skeleton_instance_component> skeletons, read<physics::transform_component> transforms, write<physics::kinematic_target_component> targets) -> async::task<> {
	trace::scope_guard sg{ trace_id<"animation::clip_player">() };

	const auto dt = system_clock::fixed_dt<time>() * static_cast<float>(system_clock::fixed_steps_this_frame());
	if (dt <= time{}) {
		co_return;
	}

	const auto player_ids = players.owner_ids();
	for (std::size_t i = 0; i < players.size(); ++i) {
		auto& player = players[i];

		const auto* skeleton = skeletons.find(player_ids[i]);
		if (!skeleton || !skeleton->model.valid()) {
			continue;
		}

		const auto& model = skeleton->model.resolve();
		const auto bones = model.bones();
		const auto* proxy = transforms.find(skeleton->proxy);
		if (bones.empty() || !proxy) {
			continue;
		}

		const clip_asset* clip = player.clip.valid() ? &player.clip.resolve() : nullptr;
		if (clip && d.play && player.playing) {
			advance(player, *clip, dt);
		}

		const joint_transform root{
			.position = proxy->position - rotate_vector(proxy->orientation, model.proxy().center),
			.orientation = proxy->orientation,
		};

		std::array<joint_transform, skeleton_instance_component::max_bones> joints{};
		const auto slot_count = std::min(bones.size(), joints.size());

		for (std::size_t slot = 0; slot < slot_count; ++slot) {
			const auto& bone = bones[slot];
			const auto* track = clip ? clip->track_for(bone.source_index) : nullptr;

			const auto pose = track
				? sample(*track, player.elapsed)
				: joint_pose{
					.translation = bone.joint_local_offset,
					.rotation = bone.joint_local_rotation,
				};

			const auto& parent = bone.parent == skinned_model::no_parent
				? root
				: joints[bone.parent];

			joints[slot] = compose(parent, pose.translation, pose.rotation);

			if (slot >= skeleton->bone_count) {
				continue;
			}

			auto* target = targets.find(skeleton->bones[slot]);
			if (!target) {
				continue;
			}

			const auto body = compose(joints[slot], bone.body_offset, bone.body_rotation);
			target->position = body.position;
			target->orientation = body.orientation;
		}
	}

	co_return;
}
