#pragma once

#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"
#include "Vector/Math.h"

namespace gse::physics {
	struct motion_component final : component {
		motion_component(id* id) : component(id) {}

		vec3<length> current_position;
		vec3<velocity> current_velocity;
		vec3<acceleration> current_acceleration;

		velocity max_speed = meters_per_second(1.f);
		mass mass = kilograms(1.f);

		bool affected_by_gravity = true;
		bool moving = false;
		bool airborne = false;
		bool self_controlled = false;

		velocity get_speed() const {
			return magnitude(current_velocity);
		}
	};
}
