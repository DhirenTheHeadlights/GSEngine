module gse.graphics:ragdoll_impl;

import std;

import :ragdoll;
import :animation_components;
import :skinned_model;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.assets;
import gse.physics;

auto gse::animation::ragdoll(context& ctx, data& d, const channel_read<ragdoll_request> ragdoll_in, write<skeleton_instance_component> skeletons, write<clip_player_component> players, write<physics::motion_component> motions, write<physics::collision_component> collisions, write<physics::transform_component> transforms) -> async::task<> {
	for (const auto& [character] : ragdoll_in.of<ragdoll_request>()) {
		auto* skeleton = skeletons.find(character);
		if (!skeleton || !skeleton->model.valid()) {
			continue;
		}

		skeleton->ragdolling = true;

		if (auto* proxy_collision = collisions.find(skeleton->proxy)) {
			proxy_collision->resolve_collisions = false;
		}
		if (auto* proxy_motion = motions.find(skeleton->proxy)) {
			proxy_motion->body = physics::kinematic_body{};
			proxy_motion->current_velocity = {};
			proxy_motion->angular_velocity = {};
		}

		if (auto* player = players.find(character)) {
			player->playing = false;
		}

		if (skeleton->bone_count > 0) {
			const auto* root_at_flip = transforms.find(skeleton->bones[0]);
			const auto* proxy_at_flip = transforms.find(skeleton->proxy);
			if (root_at_flip != nullptr && proxy_at_flip != nullptr) {
				d.anchors.insert_or_assign(
					character,
					ragdoll_anchor{
						.root = root_at_flip->orientation,
						.proxy = proxy_at_flip->orientation,
					}
				);
			}
		}

		const auto model = skeleton->model.resolve();
		const auto bones = model->bones();
		for (std::uint32_t i = 0; i < skeleton->bone_count && i < bones.size(); ++i) {
			const auto& bone = bones[i];

			auto* motion = motions.find(skeleton->bones[i]);
			auto* collision = collisions.find(skeleton->bones[i]);
			if (!motion || !collision) {
				continue;
			}

			motion->body = physics::dynamic_body{
				.mass = bone.mass,
			};
			collision->resolve_collisions = true;
		}
	}

	const auto owners = skeletons.owner_ids();
	for (std::size_t i = 0; i < skeletons.size(); ++i) {
		const auto& skeleton = skeletons[i];
		if (!skeleton.ragdolling || skeleton.bone_count == 0) {
			continue;
		}

		const auto anchor = d.anchors.find(owners[i]);
		if (anchor == d.anchors.end()) {
			continue;
		}

		const auto* root = transforms.find(skeleton.bones[0]);
		auto* proxy = transforms.find(skeleton.proxy);
		if (root == nullptr || proxy == nullptr) {
			continue;
		}

		proxy->position = root->position;
		proxy->orientation = root->orientation * inverse(anchor->second.root) * anchor->second.proxy;

		const auto* root_motion = motions.find(skeleton.bones[0]);
		auto* proxy_motion = motions.find(skeleton.proxy);
		if (root_motion != nullptr && proxy_motion != nullptr) {
			proxy_motion->current_velocity = root_motion->current_velocity;
			proxy_motion->angular_velocity = root_motion->angular_velocity;
		}
	}

	co_return;
}
