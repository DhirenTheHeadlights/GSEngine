export module gse.examples:animation;

import std;

import gse.graphics;
import gse.physics;
import gse.runtime;
import gse.utility;

namespace gse::animation_examples {
	auto make_translation(float x, float y, float z) -> mat4 {
		return mat4{
			{ 1.f, 0.f, 0.f, 0.f },
			{ 0.f, 1.f, 0.f, 0.f },
			{ 0.f, 0.f, 1.f, 0.f },
			{ x, y, z, 1.f }
		};
	}

	auto make_rotation_z(const float angle_rad) -> mat4 {
		const float c = std::cos(angle_rad);
		const float s = std::sin(angle_rad);
		return mat4{
			{ c, s, 0.f, 0.f },
			{ -s, c, 0.f, 0.f },
			{ 0.f, 0.f, 1.f, 0.f },
			{ 0.f, 0.f, 0.f, 1.f }
		};
	}
}

export namespace gse::animation_examples {
	auto create_test_skeleton(
		const std::string& name = "test_skeleton"
	) -> id {
		constexpr auto no_parent = std::numeric_limits<std::uint16_t>::max();

		std::vector<joint> joints;

		joints.emplace_back(joint::params{
			.name = "root",
			.parent_index = no_parent,
			.local_bind = identity<float, 4, 4>(),
			.inverse_bind = identity<float, 4, 4>()
		});

		joints.emplace_back(joint::params{
			.name = "child",
			.parent_index = 0,
			.local_bind = make_translation(0.f, 2.f, 0.f),
			.inverse_bind = make_translation(0.f, -2.f, 0.f)
		});

		return animation::create_skeleton({
			.name = name,
			.joints = std::move(joints)
		});
	}

	auto create_oscillating_clip(
		const std::string& name = "oscillating_clip",
		const time length = seconds(2.f)
	) -> id {
		std::vector<joint_track> tracks;

		joint_track root_track;
		root_track.joint_index = 0;
		root_track.keys = {
			joint_keyframe{
				.time = seconds(0.f),
				.local_transform = identity<float, 4, 4>()
			},
			joint_keyframe{
				.time = seconds(0.5f),
				.local_transform = make_translation(0.f, 1.f, 0.f) * make_rotation_z(0.3f)
			},
			joint_keyframe{
				.time = seconds(1.f),
				.local_transform = identity<float, 4, 4>()
			},
			joint_keyframe{
				.time = seconds(1.5f),
				.local_transform = make_translation(0.f, -1.f, 0.f) * make_rotation_z(-0.3f)
			},
			joint_keyframe{
				.time = length,
				.local_transform = identity<float, 4, 4>()
			}
		};
		tracks.push_back(std::move(root_track));

		return animation::create_clip({
			.name = name,
			.length = length,
			.loop = true,
			.tracks = std::move(tracks)
		});
	}
}
