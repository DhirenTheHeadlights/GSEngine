#pragma once
#include <array>
#include <glm/glm.hpp>

#include "Physics/MotionComponent.h"
#include "Physics/Units/Units.h"
#include "Physics/Vector/Math.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
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

		axis_aligned_bounding_box(const vec3<length>& center, const vec3<length>& size)
			: upper_bound(center + size / 2.0f), lower_bound(center - size / 2.0f) {}

		axis_aligned_bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth)
			: axis_aligned_bounding_box(center, vec3<length>(width, height, depth)) {}

		vec3<length> upper_bound;
		vec3<length> lower_bound;

		auto set_position(const vec3<length>& center) -> void {
			const vec3<length> half_size = (upper_bound - lower_bound) / 2.0f;
			upper_bound = center + half_size;
			lower_bound = center - half_size;
		}

		auto get_center() const -> vec3<length> { return (upper_bound + lower_bound) / 2.0f; }
		auto get_size() const -> vec3<length> { return upper_bound - lower_bound; }
	};

	auto get_left_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_right_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_front_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto get_back_bound(const axis_aligned_bounding_box& bounding_box) -> vec3<length>;
	auto set_position_lower_bound(physics::motion_component* component, axis_aligned_bounding_box& bounding_box, const vec3<length>& new_position) -> void;

	struct oriented_bounding_box {
		oriented_bounding_box() = default;

		oriented_bounding_box(const axis_aligned_bounding_box& aabb, const glm::quat& orientation = glm::quat())
			: center(aabb.get_center()), size(aabb.get_size()), orientation(orientation) {}

		vec3<length> center;
		vec3<length> size;
		glm::quat orientation;
		std::array<vec3<length>, 3> axes;

		auto update_axes() -> void {
			const auto rotation_matrix = mat3_cast(orientation);
			axes[0] = rotation_matrix[0];
			axes[1] = rotation_matrix[1];
			axes[2] = rotation_matrix[2];
		}

		auto get_corners() const -> std::array<vec3<length>, 8> {
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
	};
}
