#pragma once

#include "Graphics/3D/BoundingBox.h"
#include "Physics/Collision/CollisionComponent.h"

namespace gse::narrow_phase_collision {
	auto resolve_collision(const physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const
	                       physics::collision_component& other_collision_component) -> void;
}