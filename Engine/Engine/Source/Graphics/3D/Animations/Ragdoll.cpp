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

auto gse::animation::ragdoll(context& ctx, data& d, const channel_read<ragdoll_request> ragdoll_in, write<skeleton_instance_component> skeletons, write<clip_player_component> players, const channel_write<physics::set_request<^^physics::transform_component::position>, physics::set_request<^^physics::transform_component::orientation>, physics::set_request<^^physics::motion_component::current_velocity>, physics::set_request<^^physics::motion_component::angular_velocity>, physics::set_request<^^physics::motion_component::body>, physics::set_request<^^physics::collision_component::resolve_collisions>> physics_out, read<physics::motion_component> motions, read<physics::collision_component> collisions, read<physics::transform_component> transforms) -> async::task<> {
	for (const auto& [character] : ragdoll_in.of<ragdoll_request>()) {
		auto* skeleton = skeletons.find(character);
		if (!skeleton || !skeleton->model.valid()) {
			continue;
		}

		skeleton->ragdolling = true;

		physics_out.push<physics::set_request<^^physics::collision_component::resolve_collisions>>({
			.target = skeleton->proxy,
			.value = false,
		});
		physics_out.push<physics::set_request<^^physics::motion_component::body>>({
			.target = skeleton->proxy,
			.value = physics::kinematic_body{},
		});
		physics_out.push<physics::set_request<^^physics::motion_component::current_velocity>>({
			.target = skeleton->proxy,
			.value = {},
		});
		physics_out.push<physics::set_request<^^physics::motion_component::angular_velocity>>({
			.target = skeleton->proxy,
			.value = {},
		});

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

			if (!motions.find(skeleton->bones[i]) || !collisions.find(skeleton->bones[i])) {
				continue;
			}

			physics_out.push<physics::set_request<^^physics::motion_component::body>>({
				.target = skeleton->bones[i],
				.value = physics::dynamic_body{
					.mass = bone.mass,
				},
			});
			physics_out.push<physics::set_request<^^physics::collision_component::resolve_collisions>>({
				.target = skeleton->bones[i],
				.value = true,
			});
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
		if (root == nullptr || transforms.find(skeleton.proxy) == nullptr) {
			continue;
		}

		physics_out.push<physics::set_request<^^physics::transform_component::position>>({
			.target = skeleton.proxy,
			.value = root->position,
		});
		physics_out.push<physics::set_request<^^physics::transform_component::orientation>>({
			.target = skeleton.proxy,
			.value = root->orientation * inverse(anchor->second.root) * anchor->second.proxy,
		});

		if (const auto* root_motion = motions.find(skeleton.bones[0]); root_motion != nullptr && motions.find(skeleton.proxy) != nullptr) {
			physics_out.push<physics::set_request<^^physics::motion_component::current_velocity>>({
				.target = skeleton.proxy,
				.value = root_motion->current_velocity,
			});
			physics_out.push<physics::set_request<^^physics::motion_component::angular_velocity>>({
				.target = skeleton.proxy,
				.value = root_motion->angular_velocity,
			});
		}
	}

	co_return;
}
