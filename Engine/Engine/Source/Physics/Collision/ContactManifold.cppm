export module gse.physics:contact_manifold;

import std;

import gse.math;

export namespace gse {
	enum class feature_type : std::uint8_t {
		vertex,
		edge,
		face
	};

	struct feature_id {
		feature_type type_a = feature_type::face;
		feature_type type_b = feature_type::face;
		std::uint8_t index_a = 0;
		std::uint8_t index_b = 0;

		auto operator==(const feature_id&) const -> bool = default;
	};

	struct contact_point {
		vec3<length> position_on_a;
		vec3<length> position_on_b;
		unitless::vec3 normal;
		length separation;
		feature_id feature;
	};

	struct contact_manifold {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::array<contact_point, 4> points;
		std::uint32_t point_count = 0;
		unitless::vec3 tangent_u;
		unitless::vec3 tangent_v;

		auto add_point(const contact_point& p) -> bool;
		auto clear() -> void;
	};

	auto compute_tangent_basis(
		const unitless::vec3& normal
	) -> std::pair<unitless::vec3, unitless::vec3>;
}

namespace gse {
	struct feature_id_hash {
		auto operator()(const feature_id& fid) const noexcept -> std::size_t {
			std::size_t h = 0;
			h ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(fid.type_a)) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(fid.type_b)) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<std::uint8_t>{}(fid.index_a) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<std::uint8_t>{}(fid.index_b) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
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

auto gse::compute_tangent_basis(const unitless::vec3& normal) -> std::pair<unitless::vec3, unitless::vec3> {
	unitless::vec3 u;
	if (std::abs(normal.x()) < 0.9f) {
		u = cross(normal, unitless::vec3(1.f, 0.f, 0.f));
	} else {
		u = cross(normal, unitless::vec3(0.f, 1.f, 0.f));
	}
	u = normalize(u);
	const unitless::vec3 v = cross(normal, u);
	return { u, v };
}
