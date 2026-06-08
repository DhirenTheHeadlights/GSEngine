export module gse.physics:skeleton;

import std;

import :bounding_box;
import :joint_spec;

import gse.math;
import gse.meta;

export namespace gse::physics {
	constexpr std::uint16_t no_bone = std::numeric_limits<std::uint16_t>::max();

	struct bone {
		std::string name;
		std::uint16_t parent_index = no_bone;
		vec3<displacement> local_offset;
		quat local_rotation = quat(1.f, 0.f, 0.f, 0.f);
		bone_shape shape;
		mass mass = kilograms(1.f);
	};

	struct skeletal_joint {
		std::uint16_t bone_a = no_bone;
		std::uint16_t bone_b = no_bone;
		joint_config config;
	};

	struct muscle {
		std::uint16_t bone_a = no_bone;
		std::uint16_t bone_b = no_bone;
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		length rest_length;
		force max_force = newtons(3200.f);
	};

	struct skeleton {
		std::string name;
		std::vector<bone> bones;
		std::vector<skeletal_joint> joints;
		std::vector<muscle> muscles;
	};

	auto bone_index_of(
		const skeleton& s,
		std::string_view name
	) -> std::uint16_t;

	auto volume_of(
		const bone_shape& shape
	) -> volume;

	auto mass_from_density(
		const bone_shape& shape,
		density d
	) -> mass;

	auto moment_of_inertia_of(
		const bone_shape& shape,
		mass m
	) -> inertia;
}

auto gse::physics::bone_index_of(const skeleton& s, const std::string_view name) -> std::uint16_t {
	for (std::size_t i = 0; i < s.bones.size(); ++i) {
		if (s.bones[i].name == name) {
			return static_cast<std::uint16_t>(i);
		}
	}
	return no_bone;
}

auto gse::physics::volume_of(const bone_shape& shape) -> volume {
	volume result;
	gse::match(shape)
		.if_is([&](const box_shape& s) {
			result = s.size.x() * s.size.y() * s.size.z();
		})
		.else_if_is([&](const sphere_shape& s) {
			const float k = 4.f / 3.f * std::numbers::pi_v<float>;
			result = k * s.radius * s.radius * s.radius;
		})
		.else_if_is([&](const capsule_shape& s) {
			const float pi = std::numbers::pi_v<float>;
			const auto cylinder = 2.f * pi * s.radius * s.radius * s.half_height;
			const auto caps = 4.f / 3.f * pi * s.radius * s.radius * s.radius;
			result = cylinder + caps;
		});
	return result;
}

auto gse::physics::mass_from_density(const bone_shape& shape, const density d) -> mass {
	return volume_of(shape) * d;
}

auto gse::physics::moment_of_inertia_of(const bone_shape& shape, const mass m) -> inertia {
	inertia result;
	gse::match(shape)
		.if_is([&](const box_shape& s) {
			result = m * gse::dot(s.size, s.size) / 18.f;
		})
		.else_if_is([&](const sphere_shape& s) {
			result = m * s.radius * s.radius * (2.f / 5.f);
		})
		.else_if_is([&](const capsule_shape& s) {
			const auto axial = 2.f * s.half_height + 2.f * s.radius;
			const auto width = 2.f * s.radius;
			const vec3<length> bbox(width, axial, width);
			result = m * gse::dot(bbox, bbox) / 18.f;
		});
	return result;
}
