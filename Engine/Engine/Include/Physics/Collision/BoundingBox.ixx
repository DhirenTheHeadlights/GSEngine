module;

#include "glm/gtc/quaternion.hpp"

export module gse.physics.bounding_box;

import glm;
import std;

import gse.physics.motion_component;
import gse.physics.math.units;
import gse.physics.math.vector;
import gse.physics.math.vector_math;

export namespace gse {
	struct collision_information {
		bool colliding = false;
		vec3<> collision_normal;
		length penetration;
		vec3<length> collision_point;

		auto get_axis() const -> int {
			if (!epsilon_equal_index(collision_normal, vec3(), x)) { return x; }
			if (!epsilon_equal_index(collision_normal, vec3(), y)) { return y; }
			return z; // Assume it is the z axis
		}
	};

	struct axis_aligned_bounding_box {
		axis_aligned_bounding_box() = default;
		axis_aligned_bounding_box(const vec3<length>& center, const vec3<length>& size);
		axis_aligned_bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth);

		vec3<length> upper_bound;
		vec3<length> lower_bound;

		auto set_position(const vec3<length>& center) -> void;

		auto get_center() const -> vec3<length> ;
		auto get_size() const -> vec3<length> ;
	};

	auto get_left_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_right_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_front_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_back_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto set_position_lower_bound(physics::motion_component* component, axis_aligned_bounding_box& bounding_box, const vec3<length>& new_position) -> void;

	struct oriented_bounding_box {
		oriented_bounding_box() = default;
		oriented_bounding_box(const axis_aligned_bounding_box& aabb, const glm::quat& orientation = glm::quat());

		vec3<length> center;
		vec3<length> size;
		glm::quat orientation;
		std::array<vec3<length>, 3> axes;

		auto update_axes() -> void;
		auto get_corners() const -> std::array<vec3<length>, 8>;
	};
}

auto gse::get_left_bound(const axis_aligned_bounding_box& bounding_box) -> gse::vec3<gse::length> {
	const vec3<length> center = bounding_box.get_center();
	const length half_width = meters(bounding_box.get_size().as<units::meters>().x / 2.0f);
	return center - vec3<units::meters>(half_width, 0.0f, 0.0f);
}

auto gse::get_right_bound(const axis_aligned_bounding_box& bounding_box) -> gse::vec3<gse::length> {
	const vec3<length> center = bounding_box.get_center();
	const length half_width = meters(bounding_box.get_size().as<units::meters>().x / 2.0f);
	return center + vec3<units::meters>(half_width, 0.0f, 0.0f);
}

auto gse::get_front_bound(const axis_aligned_bounding_box& bounding_box) -> gse::vec3<gse::length> {
	const vec3<length> center = bounding_box.get_center();
	const length half_depth = meters(bounding_box.get_size().as<units::meters>().z / 2.0f);
	return center - vec3<units::meters>(0.0f, 0.0f, half_depth);
}

auto gse::get_back_bound(const axis_aligned_bounding_box& bounding_box) -> gse::vec3<gse::length> {
	const vec3<length> center = bounding_box.get_center();
	const length half_depth = meters(bounding_box.get_size().as<units::meters>().z / 2.0f);
	return center + vec3<units::meters>(0.0f, 0.0f, half_depth);
}

auto gse::set_position_lower_bound(physics::motion_component* component, axis_aligned_bounding_box& bounding_box, const vec3<length>& new_position) -> void {
	const vec3<length> half_size = (bounding_box.upper_bound - bounding_box.lower_bound) / 2.0f;
	bounding_box.upper_bound = new_position + half_size;
	bounding_box.lower_bound = new_position - half_size;

	component->current_position = bounding_box.get_center();
}

/// AABB

gse::axis_aligned_bounding_box::axis_aligned_bounding_box(const vec3<length>& center, const vec3<length>& size)
	: upper_bound(center + size / 2.0f), lower_bound(center - size / 2.0f) {
}

gse::axis_aligned_bounding_box::axis_aligned_bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth)
	: axis_aligned_bounding_box(center, vec3<length>(width, height, depth)) {
}

auto gse::axis_aligned_bounding_box::set_position(const vec3<length>& center) -> void {
	const vec3<length> half_size = (upper_bound - lower_bound) / 2.0f;
	upper_bound = center + half_size;
	lower_bound = center - half_size;
}

auto gse::axis_aligned_bounding_box::get_center() const -> vec3<length> {
	return (upper_bound + lower_bound) / 2.0f;
}

auto gse::axis_aligned_bounding_box::get_size() const -> vec3<length> {
	return upper_bound - lower_bound;
}

/// OBB

gse::oriented_bounding_box::oriented_bounding_box(const axis_aligned_bounding_box& aabb, const glm::quat& orientation)
	: center(aabb.get_center()), size(aabb.get_size()), orientation(orientation) {
}

auto gse::oriented_bounding_box::update_axes() -> void {
	const auto rotation_matrix = mat3_cast(orientation);
	axes[0] = rotation_matrix[0];
	axes[1] = rotation_matrix[1];
	axes[2] = rotation_matrix[2];
}

auto gse::oriented_bounding_box::get_corners() const -> std::array<vec3<length>, 8> {
	std::array<vec3<length>, 8> corners;

	const auto half_size = size / 2.0f;

	for (int i = 0; i < 8; ++i) {
		const auto x = i & 1 ? half_size.as_default_units().x : -half_size.as_default_units().x;
		const auto y = i & 2 ? half_size.as_default_units().y : -half_size.as_default_units().y;
		const auto z = i & 4 ? half_size.as_default_units().z : -half_size.as_default_units().z;

		corners[i] = center + axes[0] * x + axes[1] * y + axes[2] * z;
	}

	return corners;
}