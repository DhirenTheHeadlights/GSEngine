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
