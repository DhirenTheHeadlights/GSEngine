#include "Physics/Collision/BroadPhaseCollisions.h"

#include <iostream>

#include "Physics/System.h"
#include "Physics/Vector/Math.h"

void gse::broad_phase_collision::group::add_dynamic_object(const std::shared_ptr<physics::collision_component>& collision_component, const std::shared_ptr<physics::motion_component>& motion_component) {
	m_dynamic_objects.emplace_back(collision_component, motion_component);
}

void gse::broad_phase_collision::group::add_object(const std::shared_ptr<physics::collision_component>& collision_component) {
	m_objects.emplace_back(collision_component);
}

void gse::broad_phase_collision::group::remove_object(const std::shared_ptr<physics::collision_component>& collision_component) {
	std::erase_if(m_objects, [&](const object& obj) {
		return !obj.collision_component.owner_before(collision_component) && !collision_component.owner_before(obj.collision_component);
		});

	std::erase_if(m_dynamic_objects, [&](const dynamic_object& obj) {
		return !obj.collision_component.owner_before(collision_component) && !collision_component.owner_before(obj.collision_component);
		});
}

bool gse::broad_phase_collision::check_collision(const bounding_box& box1, const bounding_box& box2) {
	return box1.upper_bound.as_default_units().x > box2.lower_bound.as_default_units().x && box1.lower_bound.as_default_units().x < box2.upper_bound.as_default_units().x &&
		   box1.upper_bound.as_default_units().y > box2.lower_bound.as_default_units().y && box1.lower_bound.as_default_units().y < box2.upper_bound.as_default_units().y &&
		   box1.upper_bound.as_default_units().z > box2.lower_bound.as_default_units().z && box1.lower_bound.as_default_units().z < box2.upper_bound.as_default_units().z;
}

bool gse::broad_phase_collision::check_collision(const bounding_box& dynamic_box, const std::shared_ptr<physics::motion_component>& dynamic_motion_component, const bounding_box& other_box) {
	bounding_box expanded_box = dynamic_box;										// Create a copy
	physics::motion_component temp_component = *dynamic_motion_component;
	update_entity(&temp_component);													// Update the entity's position in the direction of its current_velocity
	expanded_box.set_position(temp_component.current_position);						// Set the expanded box's position to the updated position
	return check_collision(expanded_box, other_box);								// Check for collision with the new expanded box
}

bool gse::broad_phase_collision::check_collision(const vec3<length>& point, const bounding_box& box) {
	return point.as_default_units().x > box.lower_bound.as_default_units().x && point.as_default_units().x < box.upper_bound.as_default_units().x &&
		   point.as_default_units().y > box.lower_bound.as_default_units().y && point.as_default_units().y < box.upper_bound.as_default_units().y &&
		   point.as_default_units().z > box.lower_bound.as_default_units().z && point.as_default_units().z < box.upper_bound.as_default_units().z;
} 

bool gse::broad_phase_collision::check_collision(const std::shared_ptr<physics::collision_component>& dynamic_object_collision_component, const std::shared_ptr<physics::motion_component>& dynamic_object_motion_component, const std::shared_ptr<physics::collision_component>& other_collision_component) {
	for (auto& box1 : dynamic_object_collision_component->bounding_boxes) {
		for (auto& box2 : other_collision_component->bounding_boxes) {
			if (check_collision(box1, box2)) {
				set_collision_information(box1, box2);

				box1.collision_information.colliding = true;
				box2.collision_information.colliding = true;

				resolve_collision(box1, dynamic_object_motion_component, box2.collision_information);

				return true;
			}

			box1.collision_information = {};
			box2.collision_information = {};
		}
	}
	return false;
}

gse::collision_information gse::broad_phase_collision::calculate_collision_information(const bounding_box& box1, const bounding_box& box2) {
	collision_information collision_information;

	if (!check_collision(box1, box2)) {
		return collision_information;
	}

	// Calculate the penetration depth on each axis
	const length x_penetration = min(box1.upper_bound, box2.upper_bound, x) - max(box1.lower_bound, box2.lower_bound, x);
	const length y_penetration = min(box1.upper_bound, box2.upper_bound, y) - max(box1.lower_bound, box2.lower_bound, y);
	const length z_penetration = min(box1.upper_bound, box2.upper_bound, z) - max(box1.lower_bound, box2.lower_bound, z);

	// Find the axis with the smallest penetration
	length penetration = x_penetration;
	vec3 collision_normal(1.0f, 0.0f, 0.0f); // Default to X axis

	if (y_penetration < penetration) {
		penetration = y_penetration;
		collision_normal = vec3(0.0f, 1.0f, 0.0f);
	}
	if (z_penetration < penetration) {
		penetration = z_penetration;
		collision_normal = vec3(0.0f, 0.0f, 1.0f);
	}

	// Determine the collision normal
	if (const vec3<length> delta_center = box2.get_center() - box1.get_center(); dot(delta_center, collision_normal) < 0.0f) {
		collision_normal = -collision_normal;
	}

	// Calculate the collision point
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

void gse::broad_phase_collision::update(broad_phase_collision::group& group) {
	for (auto& dynamic_object : group.get_dynamic_objects()) {
		const auto dynamic_object_ptr = dynamic_object.collision_component.lock();
		if (!dynamic_object_ptr) {
			continue;
		}

		const auto motion_component_ptr = dynamic_object.motion_component.lock();
		if (!motion_component_ptr) {
			continue;
		}

		for (auto& object : group.get_objects()) {
			const auto object_ptr = object.collision_component.lock();
			if (!object_ptr) {
				continue;
			}

			if (dynamic_object_ptr == object_ptr) {
				continue;
			}

			if (!check_collision(dynamic_object_ptr, motion_component_ptr, object_ptr)) {
				motion_component_ptr->airborne = true;
			}
		}
	}
}

