#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Physics/MotionComponent.h"

namespace gse::broad_phase_collision {
	auto check_collision(const bounding_box& box1, const bounding_box& box2) -> bool;
	auto check_future_collision(const bounding_box& dynamic_box, const physics::motion_component& dynamic_motion_component, const bounding_box& other_box) -> bool;
	auto check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component) -> bool;
	auto calculate_collision_information(const bounding_box& box1, const bounding_box& box2) -> collision_information;
	auto set_collision_information(const bounding_box& box1, const bounding_box& box2) -> void;

	auto update() -> void;
}

