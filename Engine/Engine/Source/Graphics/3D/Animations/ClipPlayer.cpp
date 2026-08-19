module gse.graphics:clip_player_impl;

import std;

import :clip_player;
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
import gse.time;
import gse.assets;
import gse.physics;

namespace gse::animation {
	struct active_layer {
		std::shared_ptr<const clip_asset> clip;
		clip_binding binding{};
		float weight = 0.f;
	};
}

auto gse::animation::compose(const joint_transform& parent, const vec3<displacement>& offset, const quat& rotation) -> joint_transform {
	return {
		.position = parent.position + rotate_vector(parent.orientation, offset),
		.orientation = normalize(parent.orientation * rotation),
	};
}

auto gse::animation::binding_for(data& d, const skinned_model& model, const clip_asset& clip, const clip_binding_key key) -> const clip_binding& {
	if (const auto it = d.bindings.find(key); it != d.bindings.end()) {
		return it->second;
	}

	clip_binding binding{};
	binding.track_by_slot.fill(clip_binding::no_track);

	const auto bones = model.bones();
	const auto tracks = clip.tracks();
	for (std::size_t slot = 0; slot < bones.size() && slot < binding.track_by_slot.size(); ++slot) {
		const auto match = std::ranges::find(tracks, bones[slot].name, &joint_track::joint_name);
		if (match == tracks.end()) {
			continue;
		}
		binding.track_by_slot[slot] = static_cast<std::uint16_t>(match - tracks.begin());
		if (bones[slot].parent == skinned_model::no_parent) {
			binding.ground_speed = ground_speed_of(*match, clip.length());
		}
	}

	return d.bindings.insert_or_assign(key, binding).first->second;
}

auto gse::animation::run(context& ctx, data& d, const channel_read<physics::interpolation_state> interp_in, write<clip_player_component> players, read<skeleton_instance_component> skeletons, read<physics::transform_component> transforms, read<physics::motion_component> motions, write<physics::kinematic_target_component> targets) -> async::task<> {
	trace::scope_guard sg{ trace_id<"animation::clip_player">() };

	const auto& interpolation = interp_in.of<physics::interpolation_state>();
	const int steps = interpolation.empty() ? system_clock::fixed_steps_this_frame() : interpolation[0].steps;
	const int age_steps = interpolation.empty() ? 0 : interpolation[0].readback_age_steps;
	const auto dt = system_clock::fixed_dt<time>() * static_cast<float>(steps);
	if (dt <= time{}) {
		co_return;
	}

	auto lag = system_clock::fixed_lag<time_t<float, seconds>>();
	if (!interpolation.empty() && !interpolation[0].advancing) {
		lag = time_t<float, seconds>{};
	}

	const auto player_ids = players.owner_ids();
	for (std::size_t i = 0; i < players.size(); ++i) {
		auto& player = players[i];

		const auto* skeleton = skeletons.find(player_ids[i]);
		if (!skeleton || !skeleton->model.valid()) {
			continue;
		}

		const auto model_generation = skeleton->model.acquire();
		const auto& model = model_generation.resource;
		const auto bones = model->bones();
		const auto* proxy = transforms.find(skeleton->proxy);
		if (bones.empty() || !proxy) {
			continue;
		}

		std::array<active_layer, clip_player_component::max_layers> active{};
		std::uint32_t active_count = 0;
		auto period = time{};
		auto authored = velocity{};
		float authored_weight = 0.f;

		const auto layer_count = std::min<std::size_t>(player.layer_count, player.layers.size());
		for (std::size_t l = 0; l < layer_count; ++l) {
			const auto& layer = player.layers[l];
			if (!layer.clip.valid() || layer.weight <= 0.0001f) {
				continue;
			}

			const auto clip_generation = layer.clip.acquire();
			const auto& clip = clip_generation.resource;
			if (clip->length() <= time{}) {
				continue;
			}

			active[active_count] = {
				.clip = clip,
				.binding = binding_for(
					d,
					*model,
					*clip,
					{
						.model = skeleton->model.id(),
						.model_version = model_generation.version,
						.clip = layer.clip.id(),
						.clip_version = clip_generation.version
					}
				),
				.weight = layer.weight,
			};
			period += clip->length() * layer.weight;

			if (active[active_count].binding.ground_speed > velocity{}) {
				authored += active[active_count].binding.ground_speed * layer.weight;
				authored_weight += layer.weight;
			}
			++active_count;
		}

		if (active_count == 0 || period <= time{}) {
			continue;
		}

		if (d.play && player.playing) {
			const auto moving_speed = authored_weight > 0.f ? authored / authored_weight : velocity{};
			const float rate = moving_speed > velocity{} && player.desired_speed > velocity{}
				? player.desired_speed / moving_speed
				: 1.f;

			player.phase += dt * rate / period;
			player.phase -= std::floor(player.phase);
		}

		const auto* proxy_motion = motions.find(skeleton->proxy);
		auto proxy_render = physics::interpolated_transform(*proxy, proxy_motion, vec3<displacement>{}, lag);
		if (age_steps > 0 && proxy_motion) {
			proxy_render.position += proxy_motion->current_velocity * (system_clock::fixed_dt<time>() * static_cast<float>(age_steps));
		}
		const joint_transform root{
			.position = proxy_render.position - rotate_vector(proxy_render.orientation, model->proxy().center),
			.orientation = proxy_render.orientation,
		};

		std::array<joint_transform, skeleton_instance_component::max_bones> joints{};
		const auto slot_count = std::min(bones.size(), joints.size());

		for (std::size_t slot = 0; slot < slot_count; ++slot) {
			const auto& bone = bones[slot];

			joint_pose pose{
				.translation = bone.joint_local_offset,
				.rotation = bone.joint_local_rotation,
			};

			vec3<displacement> translation;
			quat rotation(0.f, 0.f, 0.f, 0.f);
			quat reference;
			float contributed = 0.f;

			for (std::uint32_t a = 0; a < active_count; ++a) {
				const auto track_index = active[a].binding.track_by_slot[slot];
				if (track_index == clip_binding::no_track) {
					continue;
				}

				const auto& clip = active[a].clip;
				const auto sampled = sample(clip->tracks()[track_index], clip->length() * player.phase);

				const float weight = active[a].weight;
				translation += sampled.translation * weight;

				if (contributed <= 0.f) {
					reference = sampled.rotation;
				}
				const float alignment = dot(reference, sampled.rotation) < 0.f ? -weight : weight;
				rotation = rotation + sampled.rotation * alignment;
				contributed += weight;
			}

			if (contributed > 0.f) {
				pose = {
					.translation = translation / contributed,
					.rotation = normalize(rotation),
				};
			}

			if (bone.parent == skinned_model::no_parent && !player.apply_root_motion) {
				pose.translation = {
					bone.joint_local_offset.x(),
					pose.translation.y(),
					bone.joint_local_offset.z(),
				};
			}

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
