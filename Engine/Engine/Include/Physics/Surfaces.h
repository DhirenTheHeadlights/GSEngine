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
			: m_friction_coefficient(fc), m_restitution(r), m_inverse_damping(id), m_traction(t) {}

		unitless m_friction_coefficient;	 // Surface friction (controls sliding)
		unitless m_restitution;			     // Bounce factor
		time m_inverse_damping;			     // Time it takes for an object to stop moving on the surface
		unitless m_traction;				 // Grip, useful for vehicle or character movement
	};

	surface_properties get_surface_properties(const surface_type& surface_type);
}