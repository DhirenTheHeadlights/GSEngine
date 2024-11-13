#pragma once

#include <unordered_map>

#include "Core/Clock.h"
#include "Physics/Units/Units.h"

namespace Engine::Surfaces {
	enum class SurfaceType : std::uint8_t {
		Concrete,
		Grass,
		Water,
		Sand,
		Gravel,
		Asphalt,
		Count
	};

	struct SurfaceProperties {
		SurfaceProperties(const float fc, const float r, const Time id, const float t)
			: frictionCoefficient(fc), restitution(r), inverseDamping(id), traction(t) {}

		Unitless frictionCoefficient;	 // Surface friction (controls sliding)
		Unitless restitution;			 // Bounce factor
		Time inverseDamping;			 // Time it takes for an object to stop moving on the surface
		Unitless traction;				 // Grip, useful for vehicle or character movement
	};

	SurfaceProperties getSurfaceProperties(const SurfaceType& surfaceType);
}