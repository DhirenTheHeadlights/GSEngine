#pragma once

#include <glm/glm.hpp>

#include "Engine/Physics/Units/Units.h"
#include "Engine/Physics/Vector/Vec3.h"

namespace Engine::Physics {
	struct MotionComponent {
		Vec3<Length> position;
		Vec3<Velocity> velocity;
		Vec3<Acceleration> acceleration;
		Mass mass;
		bool affectedByGravity = true;							
		bool airborne = false; 									

		Velocity getSpeed() const {
			return velocity.magnitude();
		}
	};
}
