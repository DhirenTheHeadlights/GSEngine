export module gs:humanoid_skeleton;

import std;
import gse;

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
	const auto arm_hang = gse::radians(1.45f);
	const float arm_hang_cos = gse::cos(arm_hang);
	const float arm_hang_sin = gse::sin(arm_hang);
	const auto hung_shoulder_x = torso.size.x() * 0.5f + upper_arm.size.x() * 0.5f * arm_hang_cos;
	const auto hung_shoulder_y = shoulder_offset_y - upper_arm.size.x() * 0.5f * arm_hang_sin;

	s.bones.push_back({
		.name = "upper_arm_l",
		.parent_index = 1,
		.local_offset = gse::vec3<gse::displacement>(-hung_shoulder_x, hung_shoulder_y, gse::meters(0.f)),
		.local_rotation = gse::quat(gse::vec3f(0.f, 0.f, 1.f), arm_hang),
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
		.local_offset = gse::vec3<gse::displacement>(hung_shoulder_x, hung_shoulder_y, gse::meters(0.f)),
		.local_rotation = gse::quat(gse::vec3f(0.f, 0.f, 1.f), -arm_hang),
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
	const auto thigh_capsule = capsule_shape{
		.radius = thigh.size.x() * 0.5f,
		.half_height = thigh.size.y() * 0.5f - thigh.size.x() * 0.5f,
	};
	const auto shin_capsule = capsule_shape{
		.radius = shin.size.x() * 0.5f,
		.half_height = shin.size.y() * 0.5f - shin.size.x() * 0.5f,
	};

	s.bones.push_back({
		.name = "thigh_l",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(-hip_offset_x, hip_offset_y, gse::meters(0.f)),
		.shape = thigh_capsule,
		.mass = thigh.mass,
	});
	s.bones.push_back({
		.name = "shin_l",
		.parent_index = 9,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), shin_offset_y, gse::meters(0.f)),
		.shape = shin_capsule,
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
		.shape = thigh_capsule,
		.mass = thigh.mass,
	});
	s.bones.push_back({
		.name = "shin_r",
		.parent_index = 12,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), shin_offset_y, gse::meters(0.f)),
		.shape = shin_capsule,
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

	const auto toe_size = gse::vec3<gse::displacement>(foot.size.x(), foot.size.y(), gse::meters(0.10f));
	const auto toe_offset_z = -(foot.size.z() * 0.5f + toe_size.z() * 0.5f);
	s.bones.push_back({
		.name = "toe_l",
		.parent_index = 11,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), toe_offset_z),
		.shape = box_shape{
			.size = toe_size
		},
		.mass = gse::kilograms(0.3f),
	});
	s.bones.push_back({
		.name = "toe_r",
		.parent_index = 14,
		.local_offset = gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), toe_offset_z),
		.shape = box_shape{
			.size = toe_size
		},
		.mass = gse::kilograms(0.3f),
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
	const auto ankle_limits = std::make_pair(gse::degrees(-50.f), gse::degrees(30.f));
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
	add_ball(
		1,
		3,
		gse::vec3<gse::displacement>(-torso.size.x() * 0.5f, shoulder_offset_y,
									 gse::meters(0.f)),
		gse::vec3<gse::displacement>(upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f))
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
	add_ball(
		1,
		6,
		gse::vec3<gse::displacement>(torso.size.x() * 0.5f, shoulder_offset_y, gse::meters(0.f)),
		gse::vec3<gse::displacement>(-upper_arm.size.x() * 0.5f,
									 gse::meters(0.f),
									 gse::meters(0.f))
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
	add_hinge(
		10,
		11,
		gse::vec3<gse::displacement>(gse::meters(0.f), -shin.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), foot.size.y() * 0.5f, -foot_offset_z),
		x_axis,
		ankle_limits
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
	add_hinge(
		13,
		14,
		gse::vec3<gse::displacement>(gse::meters(0.f), -shin.size.y() * 0.5f, gse::meters(0.f)),
		gse::vec3<gse::displacement>(gse::meters(0.f), foot.size.y() * 0.5f, -foot_offset_z),
		x_axis,
		ankle_limits
	);

	const auto toe_limits = std::make_pair(gse::degrees(-15.f), gse::degrees(60.f));
	add_hinge(
		11,
		15,
		gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), -foot.size.z() * 0.5f),
		gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), toe_size.z() * 0.5f),
		x_axis,
		toe_limits
	);
	add_hinge(
		14,
		16,
		gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), -foot.size.z() * 0.5f),
		gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.f), toe_size.z() * 0.5f),
		x_axis,
		toe_limits
	);

	rig.controlled.push_back({
		.joint_index = 8,
		.hinge_axis = x_axis
	});
	rig.controlled.push_back({
		.joint_index = 9,
		.hinge_axis = x_axis
	});
	rig.controlled.push_back({
		.joint_index = 10,
		.hinge_axis = x_axis
	});
	rig.controlled.push_back({
		.joint_index = 11,
		.hinge_axis = x_axis
	});
	rig.controlled.push_back({
		.joint_index = 12,
		.hinge_axis = x_axis
	});
	rig.controlled.push_back({
		.joint_index = 13,
		.hinge_axis = x_axis
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
		s.registry().add_component<gse::physics::joint_drive_component>(
			joint_entity,
			{
				.target = {},
				.stiffness = {},
				.damping = 0.f,
				.max_torque = gse::newton_meters(0.f),
				.enabled = false,
			}
		);
	}

	struct arm_hold {
		std::size_t joint_index;
		gse::vec3<gse::angle> target;
		gse::vec3<gse::angular_stiffness> stiffness;
	};
	const std::array arm_holds = {
		arm_hold{
			.joint_index = 2,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(40.f),
				gse::newton_meters_per_radian(30.f),
				gse::newton_meters_per_radian(60.f)
			),
		},
		arm_hold{
			.joint_index = 3,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(60.f),
				gse::newton_meters_per_radian(0.f),
				gse::newton_meters_per_radian(0.f)
			),
		},
		arm_hold{
			.joint_index = 5,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(40.f),
				gse::newton_meters_per_radian(30.f),
				gse::newton_meters_per_radian(60.f)
			),
		},
		arm_hold{
			.joint_index = 6,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(60.f),
				gse::newton_meters_per_radian(0.f),
				gse::newton_meters_per_radian(0.f)
			),
		},
		arm_hold{
			.joint_index = 14,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(25.f),
				gse::newton_meters_per_radian(0.f),
				gse::newton_meters_per_radian(0.f)
			),
		},
		arm_hold{
			.joint_index = 15,
			.target = {},
			.stiffness = gse::vec3<gse::angular_stiffness>(
				gse::newton_meters_per_radian(25.f),
				gse::newton_meters_per_radian(0.f),
				gse::newton_meters_per_radian(0.f)
			),
		},
	};
	for (const auto& hold : arm_holds) {
		if (hold.joint_index >= handle.joint_ids.size()) {
			continue;
		}
		s.registry().add_component<gse::physics::joint_drive_component>(
			handle.joint_ids[hold.joint_index],
			{
				.target = hold.target,
				.stiffness = hold.stiffness,
				.damping = 6.f,
				.max_torque = gse::newton_meters(50.f),
				.enabled = true,
			}
		);
	}

	return handle;
}
