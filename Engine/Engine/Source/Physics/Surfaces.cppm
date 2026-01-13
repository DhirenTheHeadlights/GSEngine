export module gse.physics:surfaces;

import std;

import gse.physics.math;
import gse.utility;

export namespace gse::surface {
	enum class type : std::uint8_t {
		concrete,
		grass,
		water,
		sand,
		gravel,
		asphalt,
		count
	};

	struct property {
		property(const float fc, const float r, const time id, const float t)
			: friction_coefficient(fc), restitution(r), inverse_damping(id), traction(t) {
		}

		float friction_coefficient;		 // Surface friction (controls sliding)
		float restitution;			     // Bounce factor
		time inverse_damping;			 // Time it takes for an object to stop moving on the surface
		float traction;					 // Grip, useful for vehicle or character movement
	};

	auto properties(const type& surface_type) -> property;
}

const std::unordered_map<gse::surface::type, gse::surface::property> surface_map = {
	{ gse::surface::type::concrete,
		{ 0.8f, 0.2f, gse::seconds(0.5f), 0.9f } },
	{ gse::surface::type::grass,
		{ 0.6f, 0.1f, gse::seconds(0.5f), 0.7f } },
	{ gse::surface::type::water,
		{ 0.1f, 0.0f, gse::seconds(0.5f), 0.1f } },
	{ gse::surface::type::sand,
		{ 0.4f, 0.1f, gse::seconds(0.5f), 0.6f } },
	{ gse::surface::type::gravel,
		{ 0.5f, 0.1f, gse::seconds(0.5f), 0.8f } },
	{ gse::surface::type::asphalt,
		{ 0.7f, 0.2f, gse::seconds(0.5f), 0.8f } }
};

auto gse::surface::properties(const type& surface_type) -> property {
	return surface_map.at(surface_type);
}