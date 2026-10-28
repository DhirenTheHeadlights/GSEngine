export module sandbox:characters;

import std;
import gse;

import :character_controller;
import :orbit_camera;
import :sidearm;

export namespace sandbox {
	struct character_rig {
		gse::id character;
		gse::id proxy;
	};

	auto character_model(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::resource::handle<gse::skinned_model>;

	auto character_clips(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::animation::locomotion_blend;

	auto spawn_character(
		gse::scene& s,
		gse::id owner,
		const gse::resource::handle<gse::skinned_model>& model,
		const gse::animation::locomotion_blend& clips,
		const gse::vec3<gse::position>& origin
	) -> character_rig;

	auto possess_character(
		gse::scene& s,
		const character_rig& rig,
		const gse::animation::locomotion_blend& clips
	) -> void;

	auto predicted_entity_name(
		std::string_view kind,
		gse::id owner,
		std::uint32_t sequence
	) -> std::string;
}

auto sandbox::character_model(const gse::shared_view<gse::asset::data> assets_d) -> gse::resource::handle<gse::skinned_model> {
	return gse::asset::get<gse::skinned_model>(assets_d, "SkinnedModels/x_bot.v3");
}

auto sandbox::character_clips(const gse::shared_view<gse::asset::data> assets_d) -> gse::animation::locomotion_blend {
	return {
		.idle = gse::asset::get<gse::clip_asset>(assets_d, "Clips/idle"),
		.walk = {
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe_walking"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe_walking"),
		},
		.run = {
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/running"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe"),
		},
	};
}

auto sandbox::spawn_character(gse::scene& s, const gse::id owner, const gse::resource::handle<gse::skinned_model>& model, const gse::animation::locomotion_blend& clips, const gse::vec3<gse::position>& origin) -> character_rig {
	if (!model.valid()) {
		return {};
	}

	bool clips_ready = clips.idle.valid();
	for (const auto& clip : clips.walk) {
		clips_ready = clips_ready && clip.valid();
	}
	for (const auto& clip : clips.run) {
		clips_ready = clips_ready && clip.valid();
	}
	if (!clips_ready) {
		return {};
	}

	const auto rig = model.resolve();
	const auto bones = rig->bones();
	if (bones.empty()) {
		return {};
	}

	const auto& proxy = rig->proxy();
	const auto proxy_center = origin + proxy.center;
	const auto proxy_mass = gse::kilograms(78.f);

	const auto proxy_id = s.build(std::format("{}.Proxy", owner.tag()))
		.with<gse::physics::transform_component>({
			.position = proxy_center,
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::dynamic_body{
				.mass = proxy_mass,
				.update_orientation = false,
			},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::capsule_shape{
				.radius = proxy.radius,
				.half_height = proxy.half_height,
			},
		})
		.with<gse::physics::motor_component>({
			.max_force = gse::newtons(3900.f),
			.compliance = 0.02f,
		})
		.identify();

	gse::log::println(
		gse::log::category::general,
		"character {}: proxy {} spawned at ({}, {}, {}) with radius {} and half height {}",
		owner.tag(),
		proxy_id.tag(),
		proxy_center.x(),
		proxy_center.y(),
		proxy_center.z(),
		proxy.radius,
		proxy.half_height
	);

	gse::skeleton_instance_component instance{
		.model = model,
		.proxy = proxy_id,
	};

	const gse::animation::joint_transform root{
		.position = origin,
	};

	std::array<gse::animation::joint_transform, gse::skeleton_instance_component::max_bones> joints{};
	std::array<gse::animation::joint_transform, gse::skeleton_instance_component::max_bones> bodies{};
	const auto bone_count = std::min(bones.size(), joints.size());

	for (std::size_t i = 0; i < bone_count; ++i) {
		const auto& bone = bones[i];

		const auto& parent = bone.parent == gse::skinned_model::no_parent
			? root
			: joints[bone.parent];

		joints[i] = gse::animation::compose(parent, bone.joint_local_offset, bone.joint_local_rotation);

		bodies[i] = gse::animation::compose(joints[i], bone.body_offset, bone.body_rotation);
		const auto& body = bodies[i];

		instance.bones[i] = s.build(std::format("{}.{}", owner.tag(), bone.name))
			.with<gse::physics::transform_component>({
				.position = body.position,
				.orientation = body.orientation,
			})
			.with<gse::physics::motion_component>({
				.body = gse::physics::kinematic_body{},
			})
			.with<gse::physics::collision_component>({
				.shape = bone.shape,
				.resolve_collisions = false,
			})
			.with<gse::physics::kinematic_target_component>({
				.position = body.position,
				.orientation = body.orientation,
			})
			.identify();
	}
	instance.bone_count = static_cast<std::uint32_t>(bone_count);

	for (std::size_t i = 0; i < bone_count; ++i) {
		const auto& bone = bones[i];
		if (bone.parent == gse::skinned_model::no_parent) {
			continue;
		}

		const auto& parent_body = bodies[bone.parent];
		const auto& body = bodies[i];
		const auto pivot = joints[i].position;

		s.build(std::format("{}.{}.Joint", owner.tag(), bone.name))
			.with<gse::physics::joint_spec>({
				.entity_a = instance.bones[bone.parent],
				.entity_b = instance.bones[i],
				.config = gse::physics::ball_joint{
					.anchor_a = gse::inverse_rotate_vector(parent_body.orientation, pivot - parent_body.position),
					.anchor_b = gse::inverse_rotate_vector(body.orientation, pivot - body.position),
				},
			});
	}

	return {
		.character = s.build(owner)
			.with<gse::skeleton_instance_component>(instance)
			.with<gse::clip_player_component>({
				.layers = { gse::clip_layer{ .clip = clips.idle, .weight = 1.f } },
				.layer_count = 1,
			})
			.identify(),
		.proxy = proxy_id,
	};
}

auto sandbox::possess_character(gse::scene& s, const character_rig& rig, const gse::animation::locomotion_blend& clips) -> void {
	s.build(rig.character)
		.with<character_controller::component>({
			.proxy = rig.proxy,
			.clips = clips,
			.possessed = true,
		})
		.with<sidearm::component>({})
		.with<orbit_camera::component>({
			.target = rig.proxy,
			.stepped_views = true,
			.active = true,
			.free_look = true,
		});
}

auto sandbox::predicted_entity_name(const std::string_view kind, const gse::id owner, const std::uint32_t sequence) -> std::string {
	return std::format("{}_{}_{}", kind, owner.number(), sequence);
}
