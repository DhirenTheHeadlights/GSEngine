export module gse.physics:bounding_box;

import std;

import :transform_component;

import gse.math;
import gse.meta;

export namespace gse::physics {
	struct box_shape {
		vec3<displacement> size;
	};

	struct sphere_shape {
		length radius;
	};

	struct capsule_shape {
		length radius;
		length half_height;
	};
}

export namespace gse {
	struct collision_information {
		bool colliding = false;
		vec3f collision_normal;
		penetration penetration;
		std::vector<vec3<position>> collision_points;

		auto axis() const -> axis;
	};
}

export namespace gse {
	struct aabb {
		vec3<position> max;
		vec3<position> min;

		auto overlaps(const aabb& other, const length margin = meters(0.f)) const -> bool {
			return min.x() - margin <= other.max.x() && max.x() + margin >= other.min.x() && min.y() - margin <= other.max.y() && max.y() + margin >= other.min.y() && min.z() - margin <= other.max.z() && max.z() + margin >= other.min.z();
		}
	};

	struct obb {
		vec3<position> center;
		vec3<displacement> size;
		quat orientation;
		std::array<vec3f, 3> axes;
	};

	class bounding_box {
	public:
		bounding_box() = default;

		bounding_box(
			const physics::transform_component& tc,
			const physics::box_shape& shape
		);

		bounding_box(
			const physics::transform_component& tc,
			const physics::sphere_shape& shape
		);

		bounding_box(
			const physics::transform_component& tc,
			const physics::capsule_shape& shape
		);

		explicit bounding_box(
			const physics::transform_component& tc
		);

		auto aabb() const -> aabb;

		auto obb() const -> obb;

		auto center() const -> vec3<position>;

		auto half_extents() const -> vec3<displacement>;

		auto face_normals() const -> std::array<vec3f, 6>;

		auto face_vertices(
			std::uint32_t face_index
		) const -> std::array<vec3<position>, 4>;

		auto obb_vertices() const -> std::vector<vec3<position>>;

		auto edge_endpoints(
			std::uint32_t edge_index
		) const -> std::pair<vec3<position>, vec3<position>>;

		static constexpr std::uint32_t edge_count = 12;

	private:
		vec3<position> m_center;
		quat m_orientation = quat(1.f, 0.f, 0.f, 0.f);
		vec3<displacement> m_half_extents;
	};
}

auto gse::collision_information::axis() const -> gse::axis {
	if (!epsilon_equal_index(collision_normal, vec3f(), static_cast<int>(axis::x))) {
		return axis::x;
	}
	if (!epsilon_equal_index(collision_normal, vec3f(), static_cast<int>(axis::y))) {
		return axis::y;
	}
	return axis::z;
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::box_shape& shape)
	: m_center(tc.position),
	  m_orientation(tc.orientation),
	  m_half_extents(
		  shape.size.x() * 0.5f,
		  shape.size.y() * 0.5f,
		  shape.size.z() * 0.5f
	  ) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::sphere_shape& shape)
	: m_center(tc.position),
	  m_orientation(tc.orientation),
	  m_half_extents(shape.radius, shape.radius, shape.radius) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::capsule_shape& shape)
	: m_center(tc.position),
	  m_orientation(tc.orientation),
	  m_half_extents(shape.radius, shape.half_height + shape.radius, shape.radius) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc)
	: m_center(tc.position),
	  m_orientation(tc.orientation),
	  m_half_extents{} {
}

auto gse::bounding_box::aabb() const -> gse::aabb {
	const auto rotation = mat3_cast(m_orientation);
	const auto extent =
		gse::abs(rotation[0]) * m_half_extents.x() +
		gse::abs(rotation[1]) * m_half_extents.y() +
		gse::abs(rotation[2]) * m_half_extents.z();
	return {
		.max = m_center + extent,
		.min = m_center - extent,
	};
}

auto gse::bounding_box::obb() const -> gse::obb {
	const auto rotation_matrix = mat3_cast(m_orientation);
	return {
		.center = m_center,
		.size = m_half_extents * 2.0f,
		.orientation = m_orientation,
		.axes = {
			rotation_matrix[0],
			rotation_matrix[1],
			rotation_matrix[2],
		},
	};
}

auto gse::bounding_box::center() const -> vec3<position> {
	return m_center;
}

auto gse::bounding_box::half_extents() const -> vec3<displacement> {
	return m_half_extents;
}

auto gse::bounding_box::face_normals() const -> std::array<vec3f, 6> {
	const auto box_obb = obb();
	return {
		box_obb.axes[0],
		-box_obb.axes[0],
		box_obb.axes[1],
		-box_obb.axes[1],
		box_obb.axes[2],
		-box_obb.axes[2]
	};
}

auto gse::bounding_box::face_vertices(const std::uint32_t face_index) const -> std::array<vec3<position>, 4> {
	const int axis_idx = face_index / 2;
	const float sign = (face_index % 2 == 0) ? 1.0f : -1.0f;

	const auto box_obb = obb();

	const auto& primary_axis = box_obb.axes[axis_idx];
	const auto& u_axis = box_obb.axes[(axis_idx + 1) % 3];
	const auto& v_axis = box_obb.axes[(axis_idx + 2) % 3];

	const auto h_u = m_half_extents[(axis_idx + 1) % 3];
	const auto h_v = m_half_extents[(axis_idx + 2) % 3];

	const vec3<position> face_center = box_obb.center + primary_axis * (m_half_extents[axis_idx] * sign);

	return {
		face_center + u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u - v_axis * h_v,
		face_center + u_axis * h_u - v_axis * h_v
	};
}

auto gse::bounding_box::obb_vertices() const -> std::vector<vec3<position>> {
	const auto box_obb = obb();
	std::vector<vec3<position>> corners(8);
	for (int i = 0; i < 8; ++i) {
		const auto x = (i & 1 ? 1 : -1) * m_half_extents.x();
		const auto y = (i & 2 ? 1 : -1) * m_half_extents.y();
		const auto z = (i & 4 ? 1 : -1) * m_half_extents.z();
		corners[i] = m_center + (box_obb.axes[0] * x + box_obb.axes[1] * y + box_obb.axes[2] * z);
	}
	return corners;
}

auto gse::bounding_box::edge_endpoints(const std::uint32_t edge_index) const -> std::pair<vec3<position>, vec3<position>> {
	const auto vertices = obb_vertices();

	static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 12> edge_indices = { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };

	const auto& [i0, i1] = edge_indices[edge_index % 12];
	return { vertices[i0], vertices[i1] };
}
