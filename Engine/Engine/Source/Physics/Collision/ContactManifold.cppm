export module gse.physics:contact_manifold;

import std;

import gse.math;

export namespace gse {
	enum class feature_type : std::uint8_t {
		vertex,
		edge,
		face
	};

	constexpr std::uint8_t feature_side_none = 0xFF;

	struct feature_id {
		feature_type type_a = feature_type::face;
		feature_type type_b = feature_type::face;
		std::uint8_t index_a = 0;
		std::uint8_t index_b = 0;
		std::uint8_t side_a0 = feature_side_none;
		std::uint8_t side_a1 = feature_side_none;
		std::uint8_t side_b0 = feature_side_none;
		std::uint8_t side_b1 = feature_side_none;

		auto operator==(
			const feature_id&
		) const -> bool = default;
	};

	struct contact_point {
		vec3<position> position_on_a;
		vec3<position> position_on_b;
		vec3f normal;
		separation separation;
		feature_id feature;
	};

	struct contact_manifold {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::array<contact_point, 4> points;
		std::uint32_t point_count = 0;
		vec3f tangent_u;
		vec3f tangent_v;

		auto add_point(
			const contact_point& p
		) -> bool;

		auto clear(
		) -> void;
	};

	auto compute_tangent_basis(
		const vec3f& normal
	) -> std::pair<vec3f, vec3f>;
}

auto gse::contact_manifold::add_point(const contact_point& p) -> bool {
	if (point_count >= 4) {
		return false;
	}
	points[point_count++] = p;
	return true;
}

auto gse::contact_manifold::clear() -> void {
	point_count = 0;
}

auto gse::compute_tangent_basis(const vec3f& normal) -> std::pair<vec3f, vec3f> {
	vec3f u;
	if (std::abs(normal.x()) < 0.9f) {
		u = cross(normal, vec3f(1.f, 0.f, 0.f));
	}
	else {
		u = cross(normal, vec3f(0.f, 1.f, 0.f));
	}
	u = normalize(u);
	const vec3f v = cross(normal, u);
	return { u, v };
}
