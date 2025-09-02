export module gse.physics:bounding_box;

import std;

import gse.physics.math;

export namespace gse {
	struct collision_information {
		bool colliding = false;
		unitless::vec3 collision_normal;
		length penetration;
		vec3<length> collision_point;

		auto axis() const -> unitless::axis {
			if (!epsilon_equal_index(collision_normal, unitless::vec3(), static_cast<int>(unitless::axis::x))) { return unitless::axis::x; }
			if (!epsilon_equal_index(collision_normal, unitless::vec3(), static_cast<int>(unitless::axis::y))) { return unitless::axis::y; }
			return unitless::axis::z; // Assume it is the z axis
		}
	};

	struct axis_aligned_bounding_box {
		axis_aligned_bounding_box() = default;
		axis_aligned_bounding_box(const vec3<length>& center, const vec3<length>& size);
		axis_aligned_bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth);

		vec3<length> upper_bound;
		vec3<length> lower_bound;

		auto set_position(const vec3<length>& center) -> void;
		auto scale(std::uint32_t scale) -> void ;

		auto center() const -> vec3<length>;
		auto size() const -> vec3<length>;
	};

	struct oriented_bounding_box {
		oriented_bounding_box() = default;
		oriented_bounding_box(const axis_aligned_bounding_box& aabb, const quat& orientation = quat());

		vec3<length> center;
		vec3<length> size;
		quat orientation;
		std::array<unitless::vec3, 3> axes;

		auto update_axes() -> void;
		auto corners() const -> std::array<vec3<length>, 8>;
		auto face_vertices(unitless::axis axis, bool positive) const -> std::array<vec3<length>, 4>;
		auto half_extents() const -> vec3<length> ;
	};
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

auto gse::axis_aligned_bounding_box::scale(const std::uint32_t scale) -> void {
	const auto c = center();
	const auto s = size() * scale;
	upper_bound = c + s / 2.0f;
	lower_bound = c - s / 2.0f;
}

auto gse::axis_aligned_bounding_box::center() const -> vec3<length> {
	return (upper_bound + lower_bound) / 2.0f;
}

auto gse::axis_aligned_bounding_box::size() const -> vec3<length> {
	return upper_bound - lower_bound;
}

/// OBB

gse::oriented_bounding_box::oriented_bounding_box(const axis_aligned_bounding_box& aabb, const quat& orientation) : center(aabb.center()), size(aabb.size()), orientation(orientation) {
	update_axes();
}

auto gse::oriented_bounding_box::update_axes() -> void {
	const auto rotation_matrix = mat3_cast(orientation);
	axes[0] = rotation_matrix[0];
	axes[1] = rotation_matrix[1];
	axes[2] = rotation_matrix[2];
}

auto gse::oriented_bounding_box::corners() const -> std::array<vec3<length>, 8> {
	std::array<vec3<length>, 8> corners;

	const auto half_size = size / 2.0f;

	for (int i = 0; i < 8; ++i) {
		const auto x = i & 1 ? half_size.x() : -half_size.x();
		const auto y = i & 2 ? half_size.y() : -half_size.y();
		const auto z = i & 4 ? half_size.z() : -half_size.z();

		corners[i] = center + (axes[0] * x + axes[1] * y + axes[2] * z);
	}

	return corners;
}

auto gse::oriented_bounding_box::face_vertices(unitless::axis axis, const bool positive) const -> std::array<vec3<length>, 4> {
	const auto half_size = size / 2.0f;

	const int i = static_cast<int>(axis);
	const int j = (i + 1) % 3;
	const int k = (i + 2) % 3;

	// Compute the face center by moving from the box center along the face axis.
	const auto face_center = center + axes[i] * (positive ? half_size[i] : -half_size[i]);

	const auto extent_j = half_size[j];
	const auto extent_k = half_size[k];

	// Compute the four vertices of the face (a rectangle)
	return {
		face_center + axes[j] * extent_j + axes[k] * extent_k,
		face_center + axes[j] * extent_j - axes[k] * extent_k,
		face_center - axes[j] * extent_j - axes[k] * extent_k,
		face_center - axes[j] * extent_j + axes[k] * extent_k
	};
}

auto gse::oriented_bounding_box::half_extents() const -> vec3<length> {
	return size / 2.0f;
}

