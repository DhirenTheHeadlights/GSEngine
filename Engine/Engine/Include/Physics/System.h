#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/Object.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	void apply_force(motion_component& component, const vec3<force>& force);
	void apply_impulse(motion_component& component, const vec3<gse::force>& force, const time& duration);
	void update_object(motion_component& component);
	void update();
	void resolve_collision(bounding_box& dynamic_bounding_box, motion_component& dynamic_motion_component, collision_information& collision_info);
}
