export module gs:test_skeletons;

import std;
import gse;

export namespace gs {
	auto test_skeleton_t() -> gse::physics::skeleton;
}

auto gs::test_skeleton_t() -> gse::physics::skeleton {
	using namespace gse::physics;

	const auto shoulder_y = gse::meters(0.30f);
	const auto shoulder_x = gse::meters(0.20f);
	const auto upper_arm_len = gse::meters(0.25f);
	const auto half_arm = upper_arm_len * 0.5f;

	skeleton s;
	s.name = "test_t";
	s.bones.reserve(3);
	s.joints.reserve(2);
	s.muscles.reserve(2);

	s.bones.push_back({
		.name = "torso",
		.parent_index = no_bone,
		.shape =
			box_shape{
				.size = gse::vec3<gse::displacement>(gse::meters(0.4f), gse::meters(0.8f), gse::meters(0.2f))
			},
		.mass = gse::kilograms(20.f),
	});

	s.bones.push_back({
		.name = "upper_arm_l",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(-shoulder_x - half_arm, shoulder_y, gse::meters(0.f)),
		.shape =
			box_shape{
				.size = gse::vec3<gse::displacement>(upper_arm_len, gse::meters(0.07f), gse::meters(0.07f))
			},
		.mass = gse::kilograms(2.f),
	});

	s.bones.push_back({
		.name = "upper_arm_r",
		.parent_index = 0,
		.local_offset = gse::vec3<gse::displacement>(shoulder_x + half_arm, shoulder_y, gse::meters(0.f)),
		.shape =
			box_shape{
				.size = gse::vec3<gse::displacement>(upper_arm_len, gse::meters(0.07f), gse::meters(0.07f))
			},
		.mass = gse::kilograms(2.f),
	});

	s.joints.push_back({
		.bone_a = 0,
		.bone_b = 1,
		.config =
			hinge_joint{
				.anchor_a = gse::vec3<gse::displacement>(-shoulder_x, shoulder_y, gse::meters(0.f)),
				.anchor_b = gse::vec3<gse::displacement>(half_arm, gse::meters(0.f), gse::meters(0.f)),
				.axis = { 0.f, 0.f, 1.f },
			},
	});

	s.joints.push_back({
		.bone_a = 0,
		.bone_b = 2,
		.config =
			hinge_joint{
				.anchor_a = gse::vec3<gse::displacement>(shoulder_x, shoulder_y, gse::meters(0.f)),
				.anchor_b = gse::vec3<gse::displacement>(-half_arm, gse::meters(0.f), gse::meters(0.f)),
				.axis = { 0.f, 0.f, 1.f },
			},
	});

	s.muscles.push_back({
		.bone_a = 0,
		.bone_b = 1,
		.anchor_a = gse::vec3<gse::displacement>(gse::meters(-0.10f), gse::meters(0.40f), gse::meters(0.f)),
		.anchor_b = gse::vec3<gse::displacement>(gse::meters(0.10f), gse::meters(0.04f), gse::meters(0.f)),
		.rest_length = gse::meters(0.10f),
	});

	s.muscles.push_back({
		.bone_a = 0,
		.bone_b = 2,
		.anchor_a = gse::vec3<gse::displacement>(gse::meters(0.10f), gse::meters(0.40f), gse::meters(0.f)),
		.anchor_b = gse::vec3<gse::displacement>(gse::meters(-0.10f), gse::meters(0.04f), gse::meters(0.f)),
		.rest_length = gse::meters(0.10f),
	});

	return s;
}
