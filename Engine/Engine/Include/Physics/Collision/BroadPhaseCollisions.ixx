export module gse.physics.broad_phase_collision;

import std;

import gse.physics.collision_component;
import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.math;
import gse.physics.narrow_phase_collisions;
import gse.physics.system;
import gse.core.object_registry;

export namespace gse::broad_phase_collision {
	auto check_collision(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> bool;
	auto check_future_collision(const axis_aligned_bounding_box& dynamic_box, const physics::motion_component* dynamic_motion_component, const  axis_aligned_bounding_box& other_box) -> bool;
	auto check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component) -> void;
	auto calculate_collision_information(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> collision_information;

	auto update() -> void;
}

auto gse::broad_phase_collision::check_collision(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> bool {
	return box1.upper_bound.x > box2.lower_bound.x && box1.lower_bound.x < box2.upper_bound.x &&
		box1.upper_bound.y > box2.lower_bound.y && box1.lower_bound.y < box2.upper_bound.y &&
		box1.upper_bound.z > box2.lower_bound.z && box1.lower_bound.z < box2.upper_bound.z;
}

auto gse::broad_phase_collision::check_future_collision(const axis_aligned_bounding_box& dynamic_box, const physics::motion_component* dynamic_motion_component, const axis_aligned_bounding_box& other_box) -> bool {
	axis_aligned_bounding_box expanded_box = dynamic_box;							// Create a copy
	physics::motion_component temp_component = *dynamic_motion_component;
	update_object(temp_component);													// Update the entity's position in the direction of its current_velocity
	expanded_box.set_position(temp_component.current_position);						// Set the expanded box's position to the updated position
	return check_collision(expanded_box, other_box);								// Check for collision with the new expanded box
}

auto gse::broad_phase_collision::check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component) -> void {
	auto* box1_motion_component = registry::get_component_ptr<physics::motion_component>(dynamic_object_collision_component.parent_id);
	const auto& box1 = dynamic_object_collision_component.bounding_box;
	const auto& box2 = other_collision_component.bounding_box;

	if (check_future_collision(box1, box1_motion_component, box2)) {
		if (dynamic_object_collision_component.resolve_collisions) {
			narrow_phase_collision::resolve_collision(box1_motion_component, dynamic_object_collision_component, other_collision_component);
		}
	}
}

auto gse::broad_phase_collision::update() -> void {
	auto objects = registry::get_components<physics::collision_component>();

	for (auto& object : objects) {
		object.collision_information = {
			.colliding = object.collision_information.colliding, // Keep the colliding state
			.collision_normal = {},
			.penetration = {},
		};

		for (auto& other : objects) {
			if (object.parent_id == other.parent_id) {
				continue;
			}

			check_collision(object, other);
		}
	}

	const auto airborne_check = [](physics::motion_component* motion_component, const physics::collision_component& collision_component) {
		if (abs(motion_component->current_position.y - motion_component->most_recent_y_collision) < meters(0.1f) && collision_component.collision_information.colliding) {
			motion_component->airborne = false;
		}
		else {
			motion_component->airborne = true;
			motion_component->most_recent_y_collision = meters(std::numeric_limits<float>::max());
		}
		};

	for (auto& object : objects) {
		if (auto* motion_component = registry::get_component_ptr<physics::motion_component>(object.parent_id)) {
			airborne_check(motion_component, object);
		}
	}
}