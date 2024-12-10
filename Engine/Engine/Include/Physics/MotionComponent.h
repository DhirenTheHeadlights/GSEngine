#pragma once

#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"
#include "Vector/Math.h"

namespace gse::Physics {
	struct MotionComponent : engine_component {
		MotionComponent(ID* id) : engine_component(id) {}

		Vec3<Length> position;
		Vec3<Velocity> velocity;
		Vec3<Acceleration> acceleration;

		Velocity maxSpeed;
		Mass mass;

		bool affectedByGravity = true;
		bool moving = false;
		bool airborne = false;
		bool selfControlled = false;

		Velocity getSpeed() const {
			return magnitude(velocity);
		}
	};
}
