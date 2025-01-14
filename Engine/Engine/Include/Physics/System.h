#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	auto apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position = { 0.f, 0.f, 0.f }) -> void;
	auto apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void;
	auto update_object(motion_component& component) -> void;
	auto update() -> void;
	auto resolve_collision(axis_aligned_bounding_box& dynamic_bounding_box, motion_component* dynamic_motion_component, collision_information&
	                       collision_info) -> void;
}
