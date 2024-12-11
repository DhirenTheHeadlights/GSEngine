#pragma once

#include <unordered_map>

#include "Core/Clock.h"
#include "Physics/Units/Units.h"

namespace gse::surfaces {
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
			: friction_coefficient(fc), restitution(r), inverse_damping(id), traction(t) {}

		unitless friction_coefficient;		 // Surface friction (controls sliding)
		unitless restitution;			     // Bounce factor
		time inverse_damping;			     // Time it takes for an object to stop moving on the surface
		unitless traction;					 // Grip, useful for vehicle or character movement
	};

	surface_properties get_surface_properties(const surface_type& surface_type);
}