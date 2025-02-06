export module gse.physics.surfaces;

import std;

import gse.physics.math;
import gse.core.clock;

export namespace gse::surfaces {
	enum class surface_type : std::uint8_t {
		concrete,
		grass,
		water,
		sand,
		gravel,
		asphalt,
		count
	};

	struct surface_properties {
		surface_properties(const float fc, const float r, const time id, const float t)
			: friction_coefficient(fc), restitution(r), inverse_damping(id), traction(t) {
		}

		float friction_coefficient;		 // Surface friction (controls sliding)
		float restitution;			     // Bounce factor
		time inverse_damping;			 // Time it takes for an object to stop moving on the surface
		float traction;					 // Grip, useful for vehicle or character movement
	};

	auto get_surface_properties(const surface_type& surface_type) -> surface_properties;
}

const std::unordered_map<gse::surfaces::surface_type, gse::surfaces::surface_properties> surface_map = {
	{ gse::surfaces::surface_type::concrete,
		{ 0.8f, 0.2f, gse::seconds(0.5f), 0.9f } },
	{ gse::surfaces::surface_type::grass,
		{ 0.6f, 0.1f, gse::seconds(0.5f), 0.7f } },
	{ gse::surfaces::surface_type::water,
		{ 0.1f, 0.0f, gse::seconds(0.5f), 0.1f } },
	{ gse::surfaces::surface_type::sand,
		{ 0.4f, 0.1f, gse::seconds(0.5f), 0.6f } },
	{ gse::surfaces::surface_type::gravel,
		{ 0.5f, 0.1f, gse::seconds(0.5f), 0.8f } },
	{ gse::surfaces::surface_type::asphalt,
		{ 0.7f, 0.2f, gse::seconds(0.5f), 0.8f } }
};

auto gse::surfaces::get_surface_properties(const surface_type& surface_type) -> surface_properties {
	return surface_map.at(surface_type);
}