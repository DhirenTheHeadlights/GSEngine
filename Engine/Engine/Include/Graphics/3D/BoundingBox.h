#pragma once
#include <glm/glm.hpp>
#include "Physics/Units/Units.h"
#include "Physics/Vector/Math.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	struct collision_information {
		bool colliding = false;
		vec3<> collision_normal;
		length penetration;
		vec3<length> collision_point;

		int get_axis() const {
			if (!epsilon_equal_index(collision_normal, vec3(), x)) { return 0; }
			if (!epsilon_equal_index(collision_normal, vec3(), y)) { return 1; }
			return z; // Assume it is the z axis
		}
	};

	struct bounding_box {
		bounding_box() = default;

		bounding_box(const vec3<length>& center, const vec3<length>& size)
			: upper_bound(center + size / 2.0f), lower_bound(center - size / 2.0f) {}

		bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth)
			: bounding_box(center, vec3<length>(width, height, depth)) {}

		vec3<length> upper_bound;
		vec3<length> lower_bound;

		mutable collision_information collision_information = {};

		void set_position(const vec3<length>& center) {
			const vec3<length> half_size = (upper_bound - lower_bound) / 2.0f;
			upper_bound = center + half_size;
			lower_bound = center - half_size;
		}

		vec3<length> get_center() const { return (upper_bound + lower_bound) / 2.0f; }
		vec3<length> get_size() const { return upper_bound - lower_bound; }
	};

	vec3<length> get_left_bound(const bounding_box& bounding_box);
	vec3<length> get_right_bound(const bounding_box& bounding_box);
	vec3<length> get_front_bound(const bounding_box& bounding_box);
	vec3<length> get_back_bound(const bounding_box& bounding_box);
	void set_bounding_box_position_by_axis(bounding_box& bounding_box, const vec3<length>& new_position, int side);
}
