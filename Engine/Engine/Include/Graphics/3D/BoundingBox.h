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
			if (!epsilonEqualIndex(collision_normal, vec3(), x)) { return 0; }
			if (!epsilonEqualIndex(collision_normal, vec3(), y)) { return 1; }
			return z; // Assume it is the z axis
		}
	};

	struct bounding_box {
		bounding_box() = default;

		// Only use this constructor if you know what you are doing
		bounding_box(const vec3<length>& upper_bound, const vec3<length>& lower_bound)
			: upper_bound(upper_bound), lower_bound(lower_bound) {}

		// Use this constructor for a centered bounding box
		bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth)
			: bounding_box(center + vec3<length>(width / 2.f, height / 2.f, depth / 2.f),
			               center - vec3<length>(width / 2.f, height / 2.f, depth / 2.f)) {}

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
}
