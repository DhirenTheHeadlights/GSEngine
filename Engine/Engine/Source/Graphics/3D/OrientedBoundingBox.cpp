module;

#include "glm/gtc/quaternion.hpp"

module gse.physics.bounding_box;

import gse.physics.math.vector;
import gse.physics.math.units;

gse::oriented_bounding_box::oriented_bounding_box(const axis_aligned_bounding_box& aabb, const glm::quat& orientation)
	: center(aabb.get_center()), size(aabb.get_size()), orientation(orientation) {}

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