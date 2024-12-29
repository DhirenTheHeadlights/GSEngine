#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace gse::broad_phase_collision {
	bool check_collision(const bounding_box& box1, const bounding_box& box2);
	bool check_future_collision(const bounding_box& dynamic_box, const physics::motion_component& dynamic_motion_component, const bounding_box&
	                            other_box);
	bool check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component);
	collision_information calculate_collision_information(const bounding_box& box1, const bounding_box& box2);
	void set_collision_information(const bounding_box& box1, const bounding_box& box2);

	void update();
}

