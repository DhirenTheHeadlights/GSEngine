export module gse.physics:broad_phase_collision;

import std;

import :collision_component;
import :motion_component;
import :bounding_box;
import :narrow_phase_collisions;
import :system;

import gse.physics.math;

export namespace gse::broad_phase_collision {
	auto check_collision(
		const axis_aligned_bounding_box& box1,
		const axis_aligned_bounding_box& box2
	) -> bool;

	auto check_future_collision(
		const axis_aligned_bounding_box& dynamic_box,
		const physics::motion_component* dynamic_motion_component,
		const axis_aligned_bounding_box& other_box,
		time dt
	) -> bool;

	auto check_collision(
		physics::collision_component& dynamic_object_collision_component,
		physics::motion_component* dynamic_object_motion_component,
		physics::collision_component& other_collision_component,
		physics::motion_component* other_motion_component,
		time dt
	) -> void;

	auto calculate_collision_information(
		const axis_aligned_bounding_box& box1,
		const axis_aligned_bounding_box& box2
	) -> collision_information;

	auto update(
		std::span<physics::collision_component> collision_components,
		std::span<physics::motion_component> motion_components,
		time dt
	) -> void;
}

auto gse::broad_phase_collision::check_collision(const axis_aligned_bounding_box& box1, const axis_aligned_bounding_box& box2) -> bool {
	return box1.upper_bound.x() > box2.lower_bound.x() && box1.lower_bound.x() < box2.upper_bound.x() &&
		box1.upper_bound.y() > box2.lower_bound.y() && box1.lower_bound.y() < box2.upper_bound.y() &&
		box1.upper_bound.z() > box2.lower_bound.z() && box1.lower_bound.z() < box2.upper_bound.z();
}

auto gse::broad_phase_collision::check_future_collision(const axis_aligned_bounding_box& dynamic_box, const physics::motion_component* dynamic_motion_component, const axis_aligned_bounding_box& other_box, const time dt) -> bool {
	axis_aligned_bounding_box expanded_box = dynamic_box;							// Create a copy
	physics::motion_component temp_component = *dynamic_motion_component;
	update_object(temp_component, dt, nullptr);										// Update the entity's position in the direction of its current_velocity
	expanded_box.set_position(temp_component.current_position);						// Set the expanded box's position to the updated position
	expanded_box.scale(std::sqrt(3));
	return check_collision(expanded_box, other_box);								// Check for collision with the new expanded box
}

auto gse::broad_phase_collision::check_collision(physics::collision_component& dynamic_object_collision_component, physics::motion_component* dynamic_object_motion_component, physics::collision_component& other_collision_component, physics::motion_component* other_motion_component, const time dt) -> void {
	const auto& box1 = dynamic_object_collision_component.bounding_box;
	const auto& box2 = other_collision_component.bounding_box;

	if (check_future_collision(box1, dynamic_object_motion_component, box2, dt)) {
		if (dynamic_object_collision_component.resolve_collisions) {
			narrow_phase_collision::resolve_collision(dynamic_object_motion_component, dynamic_object_collision_component, other_motion_component, other_collision_component);
		}
	}
}

auto gse::broad_phase_collision::update(const std::span<physics::collision_component> collision_components, const std::span<physics::motion_component> motion_components, const time dt) -> void {
	const auto airborne_check = [](
		physics::motion_component* motion_component,
		const physics::collision_component& collision_component
		) {
			if (abs(motion_component->current_position.y() - motion_component->most_recent_y_collision) < meters(0.1f) && collision_component.collision_information.colliding) {
				motion_component->airborne = false;
			}
			else {
				motion_component->airborne = true;
				motion_component->most_recent_y_collision = meters(std::numeric_limits<float>::max());
			}
		};

	for (auto& collision_component : collision_components) {
		collision_component.collision_information = {
			.colliding = collision_component.collision_information.colliding, // Keep the colliding state
			.collision_normal = {},
			.penetration = {},
		};

		physics::motion_component* motion = nullptr;

		const auto it = std::ranges::find_if(
			motion_components,
			[&](const physics::motion_component& motion_component) {
				return motion_component.owner_id() == collision_component.owner_id();
			}
		);

		if (it != motion_components.end()) {
			motion = &*it;
			collision_component.bounding_box.set_position(
				motion->current_position
			);
		}

		for (auto& other_collision_component : collision_components) {
			if (collision_component.owner_id() == other_collision_component.owner_id()) {
				continue;
			}

			const auto it2 = std::ranges::find_if(
				motion_components,
				[&](const physics::motion_component& motion_component) {
					return motion_component.owner_id() == other_collision_component.owner_id();
				}
			);

			physics::motion_component* other_motion = nullptr;

			if (it != motion_components.end()) {
				other_motion = &*it;
				collision_component.bounding_box.set_position(
					motion->current_position
				);
			}

			check_collision(collision_component, motion, other_collision_component, other_motion, dt);
		}

		if (motion) {
			airborne_check(motion, collision_component);
		}
	}
}
