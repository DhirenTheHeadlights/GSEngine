export module gse.physics:broad_phase_collision;

import std;

import :collision_component;
import :motion_component;
import :bounding_box;
import :narrow_phase_collision;
import :system;

import gse.physics.math;

export namespace gse::broad_phase_collision {
	auto check_collision(
		const bounding_box& box1,
		const bounding_box& box2
	) -> bool;

	auto check_future_collision(
		const bounding_box& dynamic_box,
		const physics::motion_component* dynamic_motion_component,
		const bounding_box& other_box
	) -> bool;

	auto check_collision(
		physics::collision_component& dynamic_object_collision_component,
		physics::motion_component* dynamic_object_motion_component,
		const physics::collision_component& other_collision_component,
		physics::motion_component* other_motion_component
	) -> void;

	auto calculate_collision_information(
		const bounding_box& box1,
		const bounding_box& box2
	) -> collision_information;

	auto update(
		registry& registry
	) -> void;
}

auto gse::broad_phase_collision::check_collision(const bounding_box& box1, const bounding_box& box2) -> bool {
	return {
		box1.aabb().max.x() > box2.aabb().min.x() && box1.aabb().min.x() < box2.aabb().max.x() &&
		box1.aabb().max.y() > box2.aabb().min.y() && box1.aabb().min.y() < box2.aabb().max.y() &&
		box1.aabb().max.z() > box2.aabb().min.z() && box1.aabb().min.z() < box2.aabb().max.z()
	};
}

auto gse::broad_phase_collision::check_future_collision(const bounding_box& dynamic_box, const physics::motion_component* dynamic_motion_component, const bounding_box& other_box) -> bool {
	const auto dt = gse::system_clock::constant_update_time<time_t<float, seconds>>();
	const auto p_pred = dynamic_motion_component->current_position + dynamic_motion_component->current_velocity * dt;

	const auto w = dynamic_motion_component->angular_velocity.as<radians_per_second>();
	const quat omega{ 0.f, w.x(), w.y(), w.z() };
	const auto dq = 0.5f * omega * dynamic_motion_component->orientation;
	const auto q_pred = normalize(dynamic_motion_component->orientation + dq * dt.as<seconds>());

	bounding_box pred_bb(dynamic_box.center(), dynamic_box.size(), 1);
	pred_bb.set_scale(dynamic_box.scale());
	pred_bb.update(p_pred, q_pred);

	const auto now_aabb = dynamic_box.aabb();
	const auto next_aabb = pred_bb.aabb();

	const auto swept_min = min(now_aabb.min, next_aabb.min);
	const auto swept_max = max(now_aabb.max, next_aabb.max);

	const auto other = other_box.aabb();

	return
		swept_max.x() > other.min.x() && swept_min.x() < other.max.x() &&
		swept_max.y() > other.min.y() && swept_min.y() < other.max.y() &&
		swept_max.z() > other.min.z() && swept_min.z() < other.max.z();
}

auto gse::broad_phase_collision::check_collision(physics::collision_component& dynamic_object_collision_component, physics::motion_component* dynamic_object_motion_component, const physics::collision_component& other_collision_component, physics::motion_component* other_motion_component) -> void {
	const auto& box1 = dynamic_object_collision_component.bounding_box;
	const auto& box2 = other_collision_component.bounding_box;

	if (check_future_collision(box1, dynamic_object_motion_component, box2)) {
		if (dynamic_object_collision_component.resolve_collisions) {
			narrow_phase_collision::resolve_collision(dynamic_object_motion_component, dynamic_object_collision_component, other_motion_component, other_collision_component);
		}
	}
}

auto gse::broad_phase_collision::update(registry& registry) -> void {
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

	for (auto collision_components = registry.linked_objects_write<physics::collision_component>(); auto& collision_component : collision_components) {
		if (!collision_component.resolve_collisions) {
			continue;
		}

		collision_component.collision_information = {
			.colliding = collision_component.collision_information.colliding,
			.collision_normal = {},
			.penetration = {},
		};

		auto* motion = registry.try_linked_object_write<physics::motion_component>(collision_component.owner_id());

		for (auto& other_collision_component : collision_components) {
			if (collision_component.owner_id() == other_collision_component.owner_id()) {
				continue;
			}

			if (!other_collision_component.resolve_collisions) {
				continue;
			}

			auto* other_motion = registry.try_linked_object_write<physics::motion_component>(other_collision_component.owner_id());

			check_collision(collision_component, motion, other_collision_component, other_motion);
		}

		if (motion) {
			airborne_check(motion, collision_component);
		}
	}
}
