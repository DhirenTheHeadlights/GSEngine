export module gse.physics:bounding_box;

import std;

import :convex_hull;
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

	struct hull_shape {
		std::uint32_t index = 0;
	};

	struct collision_shape {
		using variant_type = std::variant<box_shape, sphere_shape, capsule_shape, hull_shape>;

		variant_type value;

		collision_shape();
		collision_shape(box_shape shape);
		collision_shape(sphere_shape shape);
		collision_shape(capsule_shape shape);
		collision_shape(hull_shape shape);
		collision_shape(const collision_shape&) = default;
		collision_shape(collision_shape&&) = default;
		auto operator=(const collision_shape&) -> collision_shape& = default;
		auto operator=(collision_shape&&) -> collision_shape& = default;
		~collision_shape() = default;

		auto index() const -> std::size_t;
	};

	using bone_shape = collision_shape;

	struct mass_properties {
		mat3<inverse_inertia> inv_inertia_body;
		vec3<displacement> centroid;
	};

	auto inverse_diagonal_inertia(
		const vec3<inertia>& moments
	) -> mat3<inverse_inertia>;

	auto mass_properties_of(
		const collision_shape& shape,
		mass m,
		const convex_hull* hull
	) -> mass_properties;
}

export namespace gse {
	auto match(
		physics::collision_shape& shape
	) -> variant<physics::collision_shape::variant_type&>;

	auto match(
		const physics::collision_shape& shape
	) -> variant<const physics::collision_shape::variant_type&>;
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
			return min.x() - margin <= other.max.x() && max.x() + margin >= other.min.x() &&
				min.y() - margin <= other.max.y() && max.y() + margin >= other.min.y() &&
				min.z() - margin <= other.max.z() && max.z() + margin >= other.min.z();
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

		bounding_box(
			const physics::transform_component& tc,
			const physics::convex_hull& hull
		);

		explicit bounding_box(
			const physics::transform_component& tc
		);

		bounding_box(const bounding_box&);
		bounding_box(bounding_box&&);
		auto operator=(const bounding_box&) -> bounding_box&;
		auto operator=(bounding_box&&) -> bounding_box&;
		~bounding_box();

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

gse::physics::collision_shape::collision_shape()
	: value(box_shape{}) {
}

gse::physics::collision_shape::collision_shape(const box_shape shape)
	: value(shape) {
}

gse::physics::collision_shape::collision_shape(const sphere_shape shape)
	: value(shape) {
}

gse::physics::collision_shape::collision_shape(const capsule_shape shape)
	: value(shape) {
}

gse::physics::collision_shape::collision_shape(const hull_shape shape)
	: value(shape) {
}

auto gse::physics::collision_shape::index() const -> std::size_t {
	return value.index();
}

auto gse::match(physics::collision_shape& shape) -> variant<physics::collision_shape::variant_type&> {
	return variant<physics::collision_shape::variant_type&>(shape.value);
}

auto gse::match(const physics::collision_shape& shape) -> variant<const physics::collision_shape::variant_type&> {
	return variant<const physics::collision_shape::variant_type&>(shape.value);
}

auto gse::physics::inverse_diagonal_inertia(const vec3<inertia>& moments) -> mat3<inverse_inertia> {
	mat3<inverse_inertia> result;
	for (std::size_t axis = 0; axis < 3; ++axis) {
		if (moments[axis] > inertia{}) {
			result[axis][axis] = 1.f / moments[axis];
		}
	}
	return result;
}

auto gse::physics::mass_properties_of(const collision_shape& shape, const mass m, const convex_hull* hull) -> mass_properties {
	mass_properties result;
	match(shape)
		.if_is([&](const box_shape& s) {
			const auto x = s.size.x() * s.size.x();
			const auto y = s.size.y() * s.size.y();
			const auto z = s.size.z() * s.size.z();
			const inertia ix = m * (y + z) / 12.f / (rad * rad);
			const inertia iy = m * (x + z) / 12.f / (rad * rad);
			const inertia iz = m * (x + y) / 12.f / (rad * rad);
			result = {
				.inv_inertia_body = inverse_diagonal_inertia(vec3<inertia>(ix, iy, iz)),
				.centroid = {}
			};
		})
		.else_if_is([&](const sphere_shape& s) {
			const inertia i = m * s.radius * s.radius * (2.f / 5.f) / (rad * rad);
			result = {
				.inv_inertia_body = inverse_diagonal_inertia(vec3<inertia>(i, i, i)),
				.centroid = {}
			};
		})
		.else_if_is([&](const capsule_shape& s) {
			const float pi = std::numbers::pi_v<float>;
			const auto r_squared = s.radius * s.radius;
			const auto h_squared = s.half_height * s.half_height;
			const volume barrel_volume = 2.f * pi * r_squared * s.half_height;
			const volume cap_volume = 4.f / 3.f * pi * r_squared * s.radius;
			const volume total_volume = barrel_volume + cap_volume;
			if (total_volume <= volume{}) {
				return;
			}
			const mass barrel_mass = m * (barrel_volume / total_volume);
			const mass cap_mass = m - barrel_mass;
			const inertia axial = (barrel_mass * r_squared * 0.5f + cap_mass * r_squared * (2.f / 5.f)) / (rad * rad);
			const inertia transverse = (barrel_mass * (h_squared / 3.f + r_squared * 0.25f) +
				cap_mass * (h_squared + s.half_height * s.radius * 0.75f + r_squared * (2.f / 5.f))) / (rad * rad);
			result = {
				.inv_inertia_body = inverse_diagonal_inertia(vec3<inertia>(transverse, axial, transverse)),
				.centroid = {}
			};
		})
		.else_if_is([&](const hull_shape&) {
			if (hull == nullptr || !hull->valid()) {
				return;
			}
			const auto terms = integrate_hull(*hull);
			if (terms.total_volume <= volume{} || m <= mass{}) {
				return;
			}
			const auto scale = m / (terms.total_volume * kilograms_per_cubic_meter(1.f));
			result = {
				.inv_inertia_body = (terms.unit_density_tensor * scale).inverse(),
				.centroid = terms.centroid
			};
		});
	return result;
}

gse::bounding_box::bounding_box(const bounding_box&) = default;
gse::bounding_box::bounding_box(bounding_box&&) = default;
auto gse::bounding_box::operator=(const bounding_box&) -> bounding_box& = default;
auto gse::bounding_box::operator=(bounding_box&&) -> bounding_box& = default;
gse::bounding_box::~bounding_box() = default;

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
	: m_center(tc.position), m_orientation(tc.orientation), m_half_extents(shape.size.x() * 0.5f, shape.size.y() * 0.5f, shape.size.z() * 0.5f) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::sphere_shape& shape)
	: m_center(tc.position), m_orientation(tc.orientation), m_half_extents(shape.radius, shape.radius, shape.radius) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::capsule_shape& shape)
	: m_center(tc.position), m_orientation(tc.orientation), m_half_extents(shape.radius, shape.half_height + shape.radius, shape.radius) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc, const physics::convex_hull& hull)
	: m_center(tc.position), m_orientation(tc.orientation), m_half_extents(physics::hull_half_extents(hull)) {
}

gse::bounding_box::bounding_box(const physics::transform_component& tc)
	: m_center(tc.position), m_orientation(tc.orientation), m_half_extents{} {
}

auto gse::bounding_box::aabb() const -> gse::aabb {
	const auto rotation = mat3_cast(m_orientation);
	const auto extent = abs(rotation[0]) * m_half_extents.x() + abs(rotation[1]) * m_half_extents.y() +
		abs(rotation[2]) * m_half_extents.z();
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
	return { box_obb.axes[0], -box_obb.axes[0], box_obb.axes[1], -box_obb.axes[1], box_obb.axes[2], -box_obb.axes[2] };
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

	return { face_center + u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u + v_axis * h_v,
		face_center - u_axis * h_u - v_axis * h_v,
		face_center + u_axis * h_u - v_axis * h_v };
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

	static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 12> edge_indices = { { { 0, 1 },
		{ 2, 3 },
		{ 4, 5 },
		{ 6, 7 },
		{ 0, 2 },
		{ 1, 3 },
		{ 4, 6 },
		{ 5, 7 },
		{ 0, 4 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 } } };

	const auto& [i0, i1] = edge_indices[edge_index % 12];
	return { vertices[i0], vertices[i1] };
}