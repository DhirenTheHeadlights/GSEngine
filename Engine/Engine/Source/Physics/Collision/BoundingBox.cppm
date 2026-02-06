export module gse.physics:bounding_box;

import std;

import gse.physics.math;

export namespace gse {
	struct collision_information {
		bool colliding = false;
		unitless::vec3 collision_normal;
		length penetration;
		std::vector<vec3<length>> collision_points;

		auto axis() const -> unitless::axis {
			if (!epsilon_equal_index(collision_normal, unitless::vec3(), static_cast<int>(unitless::axis::x))) {
				return unitless::axis::x;
			}
			if (!epsilon_equal_index(collision_normal, unitless::vec3(), static_cast<int>(unitless::axis::y))) {
				return unitless::axis::y;
			}
			return unitless::axis::z; // Assume it is the z axis
		}
	};
}

namespace gse {
	struct aabb {
		vec3<length> max;
		vec3<length> min;
	};

	struct obb {
		vec3<length> center;
		vec3<length> size;
		quat orientation;
		std::array<unitless::vec3, 3> axes;
	};

	class bounding_box {
	public:
		bounding_box(const vec3<length>& center, const vec3<length>& size, std::uint32_t scale = 1);

		auto update(const vec3<length>& new_position, const quat& new_orientation) -> void;

		auto set_scale(float scale) -> void;

		auto aabb() const -> const aabb&;
		auto obb() const -> obb;

		auto center() const -> vec3<length>;
		auto size() const -> vec3<length>;
		auto half_extents() const -> vec3<length>;
		auto scale() const -> float;
		auto face_normals() const -> std::array<unitless::vec3, 6>;
		auto face_vertices(std::uint32_t face_index) const -> std::array<vec3<length>, 4>;
		auto get_obb_vertices() const -> std::vector<vec3<length>>;
	private:
		auto recalculate_aabb() const -> void;

		vec3<length> m_center;
		vec3<length> m_base_size;
		vec3<length> m_scaled_size;
		quat m_orientation;
		float m_scale = 1.0f;

		mutable gse::aabb m_aabb;
		mutable bool m_is_aabb_dirty = true;
	};
}

gse::bounding_box::bounding_box(const vec3<length>& center, const vec3<length>& size, const std::uint32_t scale) : m_center(center), m_base_size(size), m_scaled_size(size), m_scale(scale) {}

auto gse::bounding_box::update(const vec3<length>& new_position, const quat& new_orientation) -> void {
	m_center = new_position;
	m_orientation = new_orientation;
	m_is_aabb_dirty = true;
}

auto gse::bounding_box::set_scale(const float scale) -> void {
	m_scale = scale;
	m_scaled_size = m_base_size * m_scale;
	m_is_aabb_dirty = true;
}

auto gse::bounding_box::aabb() const -> const gse::aabb& {
	if (m_is_aabb_dirty) {
		recalculate_aabb();
	}
	return m_aabb;
}

auto gse::bounding_box::obb() const -> gse::obb {
	const auto rotation_matrix = mat3_cast(m_orientation);
	return {
		.center = m_center,
		.size = m_scaled_size,
		.orientation = m_orientation,
		.axes = { rotation_matrix[0], rotation_matrix[1], rotation_matrix[2] }
	};
}

auto gse::bounding_box::center() const -> vec3<length> {
	return m_center;
}

auto gse::bounding_box::size() const -> vec3<length> {
	return m_scaled_size;
}

auto gse::bounding_box::half_extents() const -> vec3<length> {
	return m_scaled_size / 2.0f;
}

auto gse::bounding_box::scale() const -> float {
	return m_scale;
}

auto gse::bounding_box::face_normals() const -> std::array<unitless::vec3, 6> {
	const auto obb_data = obb();
	return {
		 obb_data.axes[0],
		-obb_data.axes[0],
		 obb_data.axes[1],
		-obb_data.axes[1],
		 obb_data.axes[2],
		-obb_data.axes[2]
	};
}

auto gse::bounding_box::face_vertices(const std::uint32_t face_index) const -> std::array<vec3<length>, 4> {
	const auto half_ext = half_extents();

	const int axis_idx = face_index / 2; 
	const float sign = (face_index % 2 == 0) ? 1.0f : -1.0f;

	const auto box_obb = obb();

	const auto& primary_axis = box_obb.axes[axis_idx];
	const auto& u_axis = box_obb.axes[(axis_idx + 1) % 3]; 
	const auto& v_axis = box_obb.axes[(axis_idx + 2) % 3]; 

	const auto h_u = half_ext[(axis_idx + 1) % 3];
	const auto h_v = half_ext[(axis_idx + 2) % 3];

	const vec3<length> face_center = box_obb.center + primary_axis * (half_ext[axis_idx] * sign);

	return {
		face_center + u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u - v_axis * h_v,
		face_center + u_axis * h_u - v_axis * h_v
	};
}


auto gse::bounding_box::get_obb_vertices() const -> std::vector<vec3<length>> {
	const auto obb_data = obb();
	const auto half_size = obb_data.size / 2.0f;
	std::vector<vec3<length>> corners(8);
	for (int i = 0; i < 8; ++i) {
		const auto x = (i & 1 ? 1 : -1) * half_size.x();
		const auto y = (i & 2 ? 1 : -1) * half_size.y();
		const auto z = (i & 4 ? 1 : -1) * half_size.z();
		corners[i] = m_center + (obb_data.axes[0] * x + obb_data.axes[1] * y + obb_data.axes[2] * z);
	}
	return corners;
}

auto gse::bounding_box::recalculate_aabb() const -> void {
	const auto obb_data = obb();
	const auto half_size = obb_data.size / 2.0f;

	std::array<vec3<length>, 8> corners;
	for (int i = 0; i < 8; ++i) {
		const auto x = (i & 1 ? 1 : -1) * half_size.x();
		const auto y = (i & 2 ? 1 : -1) * half_size.y();
		const auto z = (i & 4 ? 1 : -1) * half_size.z();
		corners[i] = m_center + (obb_data.axes[0] * x + obb_data.axes[1] * y + obb_data.axes[2] * z);
	}

	vec3<length> min_corner(
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max()
	);
	vec3<length> max_corner(
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest()
	);

	for (const auto& corner : corners) {
		min_corner.x() = std::min(min_corner.x(), corner.x());
		min_corner.y() = std::min(min_corner.y(), corner.y());
		min_corner.z() = std::min(min_corner.z(), corner.z());

		max_corner.x() = std::max(max_corner.x(), corner.x());
		max_corner.y() = std::max(max_corner.y(), corner.y());
		max_corner.z() = std::max(max_corner.z(), corner.z());
	}

	m_aabb = {
		.max = max_corner,
		.min = min_corner
	};
	m_is_aabb_dirty = false;
}
