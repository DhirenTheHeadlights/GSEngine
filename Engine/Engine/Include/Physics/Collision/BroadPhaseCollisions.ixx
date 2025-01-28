export module gse.physics.broad_phase_collision;

import gse.physics.collision_component;
import gse.physics.motion_component;
import gse.physics.bounding_box;

export namespace gse::broad_phase_collision {
	auto check_collision(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> bool;
	auto check_future_collision(const axis_aligned_bounding_box& dynamic_box, const physics::motion_component* dynamic_motion_component, const  axis_aligned_bounding_box& other_box) -> bool;
	auto check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component) -> void;
	auto calculate_collision_information(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> collision_information;

	auto update() -> void;
}

