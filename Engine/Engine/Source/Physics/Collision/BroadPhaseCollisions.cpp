#include "Physics/Collision/BroadPhaseCollisions.h"

#include <iostream>

#include "Core/ObjectRegistry.h"
#include "Physics/System.h"
#include "Physics/Vector/Math.h"

bool gse::broad_phase_collision::check_collision(const bounding_box& box1, const bounding_box& box2) {
	return box1.upper_bound.as_default_units().x > box2.lower_bound.as_default_units().x && box1.lower_bound.as_default_units().x < box2.upper_bound.as_default_units().x &&
		   box1.upper_bound.as_default_units().y > box2.lower_bound.as_default_units().y && box1.lower_bound.as_default_units().y < box2.upper_bound.as_default_units().y &&
		   box1.upper_bound.as_default_units().z > box2.lower_bound.as_default_units().z && box1.lower_bound.as_default_units().z < box2.upper_bound.as_default_units().z;
}

bool gse::broad_phase_collision::check_future_collision(const bounding_box& dynamic_box, const physics::motion_component& dynamic_motion_component, const bounding_box& other_box) {
	bounding_box expanded_box = dynamic_box;										// Create a copy
	physics::motion_component temp_component = dynamic_motion_component;
	update_object(temp_component);													// Update the entity's position in the direction of its current_velocity
	expanded_box.set_position(temp_component.current_position);						// Set the expanded box's position to the updated position
	return check_collision(expanded_box, other_box);								// Check for collision with the new expanded box
}

bool gse::broad_phase_collision::check_collision(physics::collision_component& dynamic_object_collision_component, const physics::collision_component& other_collision_component) {
	bool collision_detected = false;

	auto& box1_motion_component = registry::get_component<physics::motion_component>(dynamic_object_collision_component.get_id());
	auto& box1 = dynamic_object_collision_component.bounding_box;
	const auto& box2 = other_collision_component.bounding_box;

	if (check_future_collision(box1, box1_motion_component, box2) || check_collision(box1, box2)) {
		set_collision_information(box1, box2);

		box1.collision_information.colliding = true;
		box2.collision_information.colliding = true;

		if (dynamic_object_collision_component.resolve_collisions) {
			resolve_collision(box1, box1_motion_component, box2.collision_information);
		}

		collision_detected = true;
	}

	return collision_detected;
}

gse::collision_information gse::broad_phase_collision::calculate_collision_information(const bounding_box& box1, const bounding_box& box2) {
	collision_information collision_information;

	if (!check_collision(box1, box2)) {
		return collision_information;
	}

	const length x_penetration_depth = min(box1.upper_bound, box2.upper_bound, x) - max(box1.lower_bound, box2.lower_bound, x);
	const length y_penetration_depth = min(box1.upper_bound, box2.upper_bound, y) - max(box1.lower_bound, box2.lower_bound, y);
	const length z_penetration_depth = min(box1.upper_bound, box2.upper_bound, z) - max(box1.lower_bound, box2.lower_bound, z);

	// Find the axis with the smallest penetration
	length penetration = x_penetration_depth;
	vec3 collision_normal(1.0f, 0.0f, 0.0f); // Default to X axis

	if (y_penetration_depth < penetration) {
		penetration = y_penetration_depth;
		collision_normal = vec3(0.0f, 1.0f, 0.0f);
	}
	if (z_penetration_depth < penetration) {
		penetration = z_penetration_depth;
		collision_normal = vec3(0.0f, 0.0f, 1.0f);
	}

	if (const vec3<length> delta_center = box2.get_center() - box1.get_center(); dot(delta_center, collision_normal) < 0.0f) {
		collision_normal = -collision_normal;
	}

	vec3<units::meters> collision_point = box1.get_center();
	collision_point += box1.get_size() / 2.f * collision_normal;					
	collision_point -= penetration * collision_normal;

	collision_information.collision_normal = collision_normal;
	collision_information.penetration = penetration;
	collision_information.collision_point = collision_point;

	return collision_information;
}

void gse::broad_phase_collision::set_collision_information(const bounding_box& box1, const bounding_box& box2) {
	box1.collision_information = calculate_collision_information(box1, box2);
	box2.collision_information = calculate_collision_information(box2, box1);
}

void gse::broad_phase_collision::update() {
	auto& objects = registry::get_components<physics::collision_component>();

	for (auto& object : objects) {
		object.bounding_box.collision_information = {};

		for (auto& other : objects) {
			if (object.get_id() == other.get_id()) {
				continue;
			}

			check_collision(object, other);
		}
	}

	const auto airborne_check = [](physics::motion_component& motion_component, const physics::collision_component& collision_component) {
		if (std::fabs(motion_component.current_position.as_default_units().y - motion_component.most_recent_y_collision.as_default_unit()) < 0.01f && collision_component.bounding_box.collision_information.colliding) {
			motion_component.airborne = false;
		}
		else {
			motion_component.airborne = true;
			motion_component.most_recent_y_collision = meters(std::numeric_limits<float>::max());
		}
		};

	for (auto& object : objects) {
		auto& motion_component = registry::get_component<physics::motion_component>(object.get_id());
		airborne_check(motion_component, object);
	}
}

