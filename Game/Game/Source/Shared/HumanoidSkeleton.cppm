export module gs:humanoid_skeleton;

import std;
import gse;

import :controlled_joint;
import :skeleton_spawn;

export namespace gs {
	struct controlled_joint_descriptor {
		std::uint16_t joint_index = std::numeric_limits<std::uint16_t>::max();
		gse::vec3f hinge_axis = { 0.f, 0.f, 1.f };
		std::uint16_t flexor_muscle_index = std::numeric_limits<std::uint16_t>::max();
		std::uint16_t extensor_muscle_index = std::numeric_limits<std::uint16_t>::max();
		gse::angle target_angle = gse::radians(0.f);
	};

	struct humanoid_rig {
		gse::physics::skeleton skel;
		std::vector<controlled_joint_descriptor> controlled;
	};

	auto humanoid_rig_default() -> humanoid_rig;

	auto spawn_humanoid(
		gse::scene& s,
		const gse::vec3<gse::position>& root_position,
		const gse::quat& root_orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> skeleton_handle;
}

namespace gs {
	struct bone_dims {
		gse::vec3<gse::displacement> size;
		gse::mass mass;
	};

	constexpr auto pelvis_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.30f), gse::meters(0.20f), gse::meters(0.20f)),
			.mass = gse::kilograms(3.f),
		};
	};
	constexpr auto torso_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.40f), gse::meters(0.50f), gse::meters(0.25f)),
			.mass = gse::kilograms(37.f),
		};
	};
	constexpr auto upper_arm_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.30f), gse::meters(0.10f), gse::meters(0.10f)),
			.mass = gse::kilograms(2.0f),
		};
	};
	constexpr auto forearm_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.28f), gse::meters(0.08f), gse::meters(0.08f)),
			.mass = gse::kilograms(1.5f),
		};
	};
	constexpr auto hand_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.18f), gse::meters(0.10f), gse::meters(0.04f)),
			.mass = gse::kilograms(0.5f),
		};
	};
	constexpr auto thigh_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.12f), gse::meters(0.45f), gse::meters(0.12f)),
			.mass = gse::kilograms(7.5f),
		};
	};
	constexpr auto shin_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.10f), gse::meters(0.40f), gse::meters(0.10f)),
			.mass = gse::kilograms(3.4f),
		};
	};
	constexpr auto foot_size = []() {
		return gs::bone_dims{
			.size = gse::vec3<gse::displacement>(gse::meters(0.10f), gse::meters(0.05f), gse::meters(0.25f)),
			.mass = gse::kilograms(1.f),
		};
	};
}

auto gs::humanoid_rig_default() -> humanoid_rig {
	using namespace gse::physics;

	humanoid_rig rig;
	auto& s = rig.skel;
	s.name = "humanoid";

	const auto pelvis = pelvis_size();
	const auto torso = torso_size();
	const auto upper_arm = upper_arm_size();
	const auto forearm = forearm_size();
	const auto hand = hand_size();
	const auto thigh = thigh_size();
	const auto shin = shin_size();
	const auto foot = foot_size();

	const auto torso_offset_y = pelvis.size.y() * 0.5f + torso.size.y() * 0.5f;
	const auto head_radius = gse::meters(0.12f);
	const auto head_offset_y = torso.size.y() * 0.5f + head_radius;
	const auto shoulder_offset_x = torso.size.x() * 0.5f + upper_arm.size.x() * 0.5f;
	const auto shoulder_offset_y = torso.size.y() * 0.5f - upper_arm.size.y() * 0.5f;
	const auto forearm_offset_x = upper_arm.size.x() * 0.5f + forearm.size.x() * 0.5f;
	const auto hand_offset_x = forearm.size.x() * 0.5f + hand.size.x() * 0.5f;
	const auto hip_offset_x = pelvis.size.x() * 0.25f;
	const auto hip_offset_y = -pelvis.size.y() * 0.5f - thigh.size.y() * 0.5f;
	const auto shin_offset_y = -thigh.size.y() * 0.5f - shin.size.y() * 0.5f;
	const auto foot_offset_y = -shin.size.y() * 0.5f - foot.size.y() * 0.5f;
	const auto foot_offset_z = gse::meters(0.f);

	s.bones.push_back({
		.name = "pelvis",
		.shape = box_shape{
			.size = pelvis.size
		},
		.mass = pelvis.mass
	});
	s.bones.push_back({
		.name = "torso",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), torso_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = torso.size
		},
		.mass = torso.mass,
	});
	s.bones.push_back({
		.name = "head",
		.parent_index = 1,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), head_offset_y, gse::meters(0.f)),
		.shape = sphere_shape{
			.radius = head_radius
		},
		.mass = gse::kilograms(6.f),
	});
	s.bones.push_back({
		.name = "upper_arm_l",
		.parent_index = 1,
		.local_offset = gse::vec3<gse::displacement>(-shoulder_offset_x, shoulder_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = upper_arm.size
		},
		.mass = upper_arm.mass,
	});
	s.bones.push_back({
		.name = "forearm_l",
		.parent_index = 3,
		.local_offset = gse::vec3<gse::displacement>(-forearm_offset_x, gse::meters(0.f), gse::meters(0.f)),
		.shape = box_shape{
			.size = forearm.size
		},
		.mass = forearm.mass,
	});
	s.bones.push_back({
		.name = "hand_l",
		.parent_index = 4,
		.local_offset = gse::vec3<gse::displacement>(-hand_offset_x, gse::meters(0.f), gse::meters(0.f)),
		.shape = box_shape{
			.size = hand.size
		},
		.mass = hand.mass,
	});
	s.bones.push_back({
		.name = "upper_arm_r",
		.parent_index = 1,
		.local_offset = gse::vec3<gse::displacement>(shoulder_offset_x, shoulder_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = upper_arm.size
		},
		.mass = upper_arm.mass,
	});
	s.bones.push_back({
		.name = "forearm_r",
		.parent_index = 6,
		.local_offset = gse::vec3<gse::displacement>(forearm_offset_x, gse::meters(0.f), gse::meters(0.f)),
		.shape = box_shape{
			.size = forearm.size
		},
		.mass = forearm.mass,
	});
	s.bones.push_back({
		.name = "hand_r",
		.parent_index = 7,
		.local_offset = gse::vec3<gse::displacement>(hand_offset_x, gse::meters(0.f), gse::meters(0.f)),
		.shape = box_shape{
			.size = hand.size
		},
		.mass = hand.mass,
	});
	s.bones.push_back({
		.name = "thigh_l",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(-hip_offset_x, hip_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = thigh.size
		},
		.mass = thigh.mass,
	});
	s.bones.push_back({
		.name = "shin_l",
		.parent_index = 9,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), shin_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = shin.size
		},
		.mass = shin.mass,
	});
	s.bones.push_back({
		.name = "foot_l",
		.parent_index = 10,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), foot_offset_y, foot_offset_z),
		.shape = box_shape{
			.size = foot.size
		},
		.mass = foot.mass,
	});
	s.bones.push_back({
		.name = "thigh_r",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(hip_offset_x, hip_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = thigh.size
		},
		.mass = thigh.mass,
	});
	s.bones.push_back({
		.name = "shin_r",
		.parent_index = 12,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), shin_offset_y, gse::meters(0.f)),
		.shape = box_shape{
			.size = shin.size
		},
		.mass = shin.mass,
	});
	s.bones.push_back({
		.name = "foot_r",
		.parent_index = 13,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), foot_offset_y, foot_offset_z),
		.shape = box_shape{
			.size = foot.size
		},
		.mass = foot.mass,
	});

	auto add_hinge = [&](
		std::uint16_t a,
		std::uint16_t b,
		const gse::vec3<gse::displacement>& anchor_a,
		const gse::vec3<gse::displacement>& anchor_b,
		const gse::vec3f& axis,
		std::optional<std::pair<gse::angle, gse::angle>> limits = std::nullopt
	) {
		s.joints.push_back({
			.bone_a = a,
			.bone_b = b,
			.config = hinge_joint{
				.anchor_a = anchor_a,
				.anchor_b = anchor_b,
				.axis = axis,
				.limits = limits,
			},
		});
	};

	auto add_fixed = [&](
		std::uint16_t a,
		std::uint16_t b,
		const gse::vec3<gse::displacement>& anchor_a,
		const gse::vec3<gse::displacement>& anchor_b
	) {
		s.joints.push_back({
			.bone_a = a,
			.bone_b = b,
			.config = fixed_joint{
				.anchor_a = anchor_a,
				.anchor_b = anchor_b,
			},
		});
	};

	auto add_ball = [&](
		std::uint16_t a,
		std::uint16_t b,
		const gse::vec3<gse::displacement>& anchor_a,
		const gse::vec3<gse::displacement>& anchor_b,
		const gse::vec3<gse::angular_stiffness>& rest_stiffness = {}
	) {
		s.joints.push_back({
			.bone_a = a,
			.bone_b = b,
			.config = ball_joint{
				.anchor_a = anchor_a,
				.anchor_b = anchor_b,
				.rest_stiffness = rest_stiffness,
			},
		});
	};

	const auto z_axis = gse::vec3f(0.f, 0.f, 1.f);
	const auto x_axis = gse::vec3f(1.f, 0.f, 0.f);

	const auto knee_limits = std::make_pair(gse::degrees(-150.f), gse::degrees(5.f));
	const auto shoulder_limits = std::make_pair(gse::degrees(-120.f), gse::degrees(120.f));
	const auto elbow_limits = std::make_pair(gse::degrees(-100.f), gse::degrees(100.f));

	const auto hip_rest_stiffness = gse::vec3<gse::angular_stiffness>(
		gse::newton_meters_per_radian(0.f),
		gse::newton_meters_per_radian(200.f),
		gse::newton_meters_per_radian(200.f)
	);

	add_fixed(
		0,
		1,
		gse::vec3<gse::displacement>(gse::meters(0.f), pelvis.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), -torso.size.y() * 0.5f, gse::meters(0.f))
	);
	add_fixed(
		1,
		2,
		gse::vec3<gse::displacement>(gse::meters(0.f), torso.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), -head_radius, gse::meters(0.f))
	);
	add_hinge(
		1,
		3,
		gse::vec3<gse::displacement>(-torso.size.x() * 0.5f, shoulder_offset_y,
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		z_axis,
		shoulder_limits
	);
	add_hinge(
		3,
		4,
		gse::vec3<gse::displacement>(-upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(forearm.size.x() * 0.5f, gse::meters(0.f),
									 gse::meters(0.f)),
		z_axis,
		elbow_limits
	);
	add_fixed(
		4,
		5,
		gse::vec3<gse::displacement>(-forearm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(hand.size.x() * 0.5f, gse::meters(0.f), gse::meters(0.f))
	);
	add_hinge(
		1,
		6,
		gse::vec3<gse::displacement>(torso.size.x() * 0.5f, shoulder_offset_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		z_axis,
		shoulder_limits
	);
	add_hinge(
		6,
		7,
		gse::vec3<gse::displacement>(upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(-forearm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f)),
		z_axis,
		elbow_limits
	);
	add_fixed(
		7,
		8,
		gse::vec3<gse::displacement>(forearm.size.x() * 0.5f, gse::meters(0.f),
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(-hand.size.x() * 0.5f, gse::meters(0.f), gse::meters(0.f))
	);
	add_ball(
		0,
		9,
		gse::vec3<gse::displacement>(-hip_offset_x, -pelvis.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh.size.y() * 0.5f, gse::meters(0.f)),
		hip_rest_stiffness
	);
	add_hinge(
		9,
		10,
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin.size.y() * 0.5f, gse::meters(0.f)),
		x_axis,
		knee_limits
	);
	add_fixed(
		10,
		11,
		gse::vec3<gse::displacement>(gse::meters(0.f), -shin.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), foot.size.y() * 0.5f, -foot_offset_z)
	);
	add_ball(
		0,
		12,
		gse::vec3<gse::displacement>(hip_offset_x, -pelvis.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh.size.y() * 0.5f, gse::meters(0.f)),
		hip_rest_stiffness
	);
	add_hinge(
		12,
		13,
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin.size.y() * 0.5f, gse::meters(0.f)),
		x_axis,
		knee_limits
	);
	add_fixed(
		13,
		14,
		gse::vec3<gse::displacement>(gse::meters(0.f), -shin.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), foot.size.y() * 0.5f, -foot_offset_z)
	);

	std::vector<gse::vec3<gse::displacement>> bone_world_offset(s.bones.size());
	for (std::size_t i = 0; i < s.bones.size(); ++i) {
		const auto& bn = s.bones[i];
		if (bn.parent_index == gse::physics::no_bone) {
			bone_world_offset[i] = bn.local_offset;
		}
		else {
			bone_world_offset[i] = bone_world_offset[bn.parent_index] + bn.local_offset;
		}
	}

	auto add_muscle_pair =
		[&](
		std::uint16_t a,
		std::uint16_t b,
		const gse::vec3<gse::displacement>& flexor_anchor_a,
		const gse::vec3<gse::displacement>& flexor_anchor_b,
		const gse::vec3<gse::displacement>& extensor_anchor_a,
		const gse::vec3<gse::displacement>& extensor_anchor_b,
		const gse::force max_force = gse::newtons(3200.f)
	) -> std::pair<std::uint16_t, std::uint16_t> {
		const auto flex_rest =
			gse::magnitude((bone_world_offset[a] + flexor_anchor_a) - (bone_world_offset[b] + flexor_anchor_b));
		const auto ext_rest =
			gse::magnitude((bone_world_offset[a] + extensor_anchor_a) - (bone_world_offset[b] + extensor_anchor_b));

		const auto fi = static_cast<std::uint16_t>(s.muscles.size());
		s.muscles.push_back({
			.bone_a = a,
			.bone_b = b,
			.anchor_a = flexor_anchor_a,
			.anchor_b = flexor_anchor_b,
			.rest_length = flex_rest,
			.max_force = max_force,
		});
		const auto ei = static_cast<std::uint16_t>(s.muscles.size());
		s.muscles.push_back({
			.bone_a = a,
			.bone_b = b,
			.anchor_a = extensor_anchor_a,
			.anchor_b = extensor_anchor_b,
			.rest_length = ext_rest,
			.max_force = max_force,
		});
		return { fi, ei };
	};

	const auto upper_arm_distal_offset = upper_arm.size.x() * 0.4f;
	const auto forearm_proximal_offset = forearm.size.x() * 0.4f;
	const auto thigh_distal_offset = thigh.size.y() * 0.4f;
	const auto shin_proximal_offset = shin.size.y() * 0.4f;
	const auto pelvis_anchor_x = pelvis.size.x() * 0.3f;
	const auto pelvis_anchor_y = -pelvis.size.y() * 0.4f;
	const auto torso_shoulder_x = torso.size.x() * 0.4f;
	const auto torso_shoulder_y = shoulder_offset_y;

	const auto arm_top_y = upper_arm.size.y() * 0.5f;
	const auto arm_bot_y = -upper_arm.size.y() * 0.5f;
	const auto forearm_top_y = forearm.size.y() * 0.5f;
	const auto forearm_bot_y = -forearm.size.y() * 0.5f;
	const auto torso_shoulder_top_y = torso_shoulder_y + gse::meters(0.20f);
	const auto torso_shoulder_bot_y = torso_shoulder_y - gse::meters(0.10f);
	const auto thigh_front_z = thigh.size.z() * 0.5f;
	const auto thigh_back_z = -thigh.size.z() * 0.5f;
	const auto shin_front_z = shin.size.z() * 0.5f;
	const auto shin_back_z = -shin.size.z() * 0.5f;
	const auto knee_thigh_front_z = thigh.size.z() * 0.9f;
	const auto knee_thigh_back_z = -thigh.size.z() * 0.9f;
	const auto knee_shin_front_z = shin.size.z() * 0.9f;
	const auto knee_shin_back_z = -shin.size.z() * 0.9f;
	const auto leg_muscle_max_force = gse::newtons(9000.f);

	add_muscle_pair(
		1,
		3,
		gse::vec3<gse::displacement>(-torso_shoulder_x, torso_shoulder_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-upper_arm_distal_offset, arm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-torso_shoulder_x, torso_shoulder_top_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-upper_arm_distal_offset, arm_top_y, gse::meters(0.f))
	);
	add_muscle_pair(
		3,
		4,
		gse::vec3<gse::displacement>(-upper_arm_distal_offset, arm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(forearm_proximal_offset, forearm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-upper_arm_distal_offset, arm_top_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(forearm_proximal_offset, forearm_top_y, gse::meters(0.f))
	);
	add_muscle_pair(
		1,
		6,
		gse::vec3<gse::displacement>(torso_shoulder_x, torso_shoulder_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(upper_arm_distal_offset, arm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(torso_shoulder_x, torso_shoulder_top_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(upper_arm_distal_offset, arm_top_y, gse::meters(0.f))
	);
	add_muscle_pair(
		6,
		7,
		gse::vec3<gse::displacement>(upper_arm_distal_offset, arm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-forearm_proximal_offset, forearm_bot_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(upper_arm_distal_offset, arm_top_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-forearm_proximal_offset, forearm_top_y, gse::meters(0.f))
	);
	const auto [hip_l_flex, hip_l_ext] = add_muscle_pair(
		0,
		9,
		gse::vec3<gse::displacement>(-pelvis_anchor_x, pelvis_anchor_y, thigh_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh_distal_offset, thigh_back_z),
		gse::vec3<gse::displacement>(-pelvis_anchor_x, pelvis_anchor_y, thigh_front_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh_distal_offset, thigh_front_z),
		leg_muscle_max_force
	);
	const auto [knee_l_flex, knee_l_ext] = add_muscle_pair(
		9,
		10,
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh_distal_offset, knee_thigh_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin_proximal_offset, knee_shin_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh_distal_offset, knee_thigh_front_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin_proximal_offset, knee_shin_front_z),
		leg_muscle_max_force
	);
	const auto [hip_r_flex, hip_r_ext] = add_muscle_pair(
		0,
		12,
		gse::vec3<gse::displacement>(pelvis_anchor_x, pelvis_anchor_y, thigh_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh_distal_offset, thigh_back_z),
		gse::vec3<gse::displacement>(pelvis_anchor_x, pelvis_anchor_y, thigh_front_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), thigh_distal_offset, thigh_front_z),
		leg_muscle_max_force
	);
	const auto [knee_r_flex, knee_r_ext] = add_muscle_pair(
		12,
		13,
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh_distal_offset, knee_thigh_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin_proximal_offset, knee_shin_back_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), -thigh_distal_offset, knee_thigh_front_z),
		gse::vec3<gse::displacement>(gse::meters(0.f), shin_proximal_offset, knee_shin_front_z),
		leg_muscle_max_force
	);

	rig.controlled.push_back({
		.joint_index = 8,
		.hinge_axis = x_axis,
		.flexor_muscle_index = hip_l_flex,
		.extensor_muscle_index = hip_l_ext
	});
	rig.controlled.push_back({
		.joint_index = 9,
		.hinge_axis = x_axis,
		.flexor_muscle_index = knee_l_flex,
		.extensor_muscle_index = knee_l_ext
	});
	rig.controlled.push_back({
		.joint_index = 11,
		.hinge_axis = x_axis,
		.flexor_muscle_index = hip_r_flex,
		.extensor_muscle_index = hip_r_ext
	});
	rig.controlled.push_back({
		.joint_index = 12,
		.hinge_axis = x_axis,
		.flexor_muscle_index = knee_r_flex,
		.extensor_muscle_index = knee_r_ext
	});

	return rig;
}

auto gs::spawn_humanoid(gse::scene& s, const gse::vec3<gse::position>& root_position, const gse::quat& root_orientation) -> skeleton_handle {
	const auto rig = humanoid_rig_default();
	auto handle = spawn_skeleton(s, rig.skel, root_position, root_orientation);

	for (const auto& cj : rig.controlled) {
		if (cj.joint_index >= handle.joint_ids.size()) {
			continue;
		}
		const auto joint_entity = handle.joint_ids[cj.joint_index];
		const auto flexor_id =
			cj.flexor_muscle_index < handle.muscle_ids.size() ? handle.muscle_ids[cj.flexor_muscle_index] : gse::id{};
		const auto extensor_id = cj.extensor_muscle_index < handle.muscle_ids.size()
			? handle.muscle_ids[cj.extensor_muscle_index]
			: gse::id{};
		s.registry().add_component<controlled_joint_component>(
			joint_entity,
			{
				.hinge_axis = cj.hinge_axis,
				.target_angle = cj.target_angle,
				.flexor_muscle = flexor_id,
				.extensor_muscle = extensor_id,
			}
		);
	}

	return handle;
}
