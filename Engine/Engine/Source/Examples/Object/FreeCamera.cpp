module gse.examples:free_camera_impl;

import std;

import :free_camera;

import gse.runtime;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.physics;
import gse.os;
import gse.assets;
import gse.gpu;

auto gse::free_camera::system::attach(context& ctx, data& d, write<component> cameras, structural<camera::follow_component> follows) -> async::task<> {
	for (const auto owner_id : cameras.drain(component_event::added)) {
		const auto* c = cameras.find(owner_id);
		if (!c) {
			continue;
		}
		d.owners.try_emplace(owner_id);

		const quat initial_orientation =
			normalize(quat(vec3f(0.f, 1.f, 0.f), c->yaw) * quat(vec3f(1.f, 0.f, 0.f), c->pitch));

		follows.add(
			owner_id,
			{
				.offset = vec3<length>(meters(0.f)),
				.priority = c->priority,
				.blend_in_duration = milliseconds(300),
				.active = true,
				.use_entity_position = false,
				.position = c->initial_position,
				.orientation = initial_orientation,
			}
		);
	}

	return {};
}

auto gse::free_camera::system::update(context& ctx, data& d, const shared_view<actions::data> as, const shared_view<camera::data> cam_s, write<component> cameras, write<camera::follow_component> follows, read<physics::transform_component> transforms, read<physics::collision_component> collisions, read<physics::motion_component> motions) -> async::task<> {
	const auto camera_ids = cameras.owner_ids();

	const auto& cs = actions::current_state(as);

	for (std::size_t i = 0; i < cameras.size(); ++i) {
		auto& c = cameras[i];
		const auto owner_id = camera_ids[i];
		const auto owner_it = d.owners.find(owner_id);
		if (owner_it == d.owners.end()) {
			continue;
		}
		auto& owner = owner_it->second;
		const auto& b = d.binds;

		auto* cam_follow = follows.find(owner_id);
		if (!cam_follow) {
			continue;
		}

		const bool f1_pressed = actions::pressed(b.toggle, cs, as);
		if (f1_pressed) {
			owner.detached = !owner.detached;
			if (owner.detached) {
				if (const auto* active_follow = follows.find(cam_s.active_controller_entity); active_follow && cam_s.active_controller_entity != owner_id) {
					cam_follow->position = active_follow->position + active_follow->offset;
					cam_follow->orientation = active_follow->orientation;
					const auto detach_forward = rotate_vector(active_follow->orientation, vec3f(0.f, 0.f, -1.f));
					c.yaw = radians(std::atan2(-detach_forward.x(), -detach_forward.z()));
					c.pitch = radians(std::asin(std::clamp(detach_forward.y(), -1.f, 1.f)));
				}
				cam_follow->priority = c.detached_priority;
			}
			else {
				cam_follow->priority = c.priority;
			}
		}

		const bool is_active_view = cam_s.active_controller_entity == owner_id;
		if (is_active_view && !cam_s.ui_focus) {
			const auto delta = cs.axis2_v(static_cast<std::uint16_t>(b.look_axis_id.number()));
			c.yaw -= delta.x() * c.mouse_sensitivity;
			c.pitch -= delta.y() * c.mouse_sensitivity;
			c.pitch = std::clamp(c.pitch, degrees(-89.f), degrees(89.f));
		}

		const quat orientation = normalize(quat(vec3f(0.f, 1.f, 0.f), c.yaw) * quat(vec3f(1.f, 0.f, 0.f), c.pitch));

		const auto v = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));
		const float lift = (actions::held(b.up, cs, as) ? 1.f : 0.f) - (actions::held(b.down, cs, as) ? 1.f : 0.f);

		const vec3f direction = rotate_vector(orientation, vec3f(v.x(), lift, v.y()));
		cam_follow->orientation = orientation;

		const auto move_delta = direction * c.speed * system_clock::dt();

		if (!c.collide_with_geometry) {
			cam_follow->position += move_delta;
			continue;
		}

		const auto inflation = c.collision_radius * 2.f;
		const auto col_ids = collisions.owner_ids();

		auto current_pos = cam_follow->position;
		for (std::size_t k = 0; k < collisions.size(); ++k) {
			const auto col_eid = col_ids[k];
			const auto* mc = motions.find(col_eid);
			if (!mc || !physics::is_static(*mc)) {
				continue;
			}
			const auto* col_tc = transforms.find(col_eid);
			if (!col_tc) {
				continue;
			}
			const auto* shape = std::get_if<physics::box_shape>(&collisions[k].shape.value);
			if (!shape) {
				continue;
			}
			const physics::box_shape inflated{
				.size = shape->size + vec3<displacement>(inflation, inflation, inflation)
			};
			const bounding_box bb(*col_tc, inflated);
			const auto result = narrow_phase_collision::query_obb(bb, current_pos);
			if (result.signed_distance < c.collision_skin) {
				const auto push = c.collision_skin - result.signed_distance;
				current_pos = current_pos + result.normal * push;
			}
		}

		vec3<displacement> remaining = move_delta;
		constexpr int max_slide_iterations = 3;
		for (int iter = 0; iter < max_slide_iterations; ++iter) {
			const auto remaining_length = magnitude(remaining);
			if (remaining_length < meters(1e-5f)) {
				break;
			}
			const vec3f seg_dir = normalize(remaining);
			const auto seg_end = current_pos + remaining;

			std::optional<narrow_phase_collision::segment_obb_hit> closest_hit;
			for (std::size_t k = 0; k < collisions.size(); ++k) {
				const auto col_eid = col_ids[k];
				const auto* mc = motions.find(col_eid);
				if (!mc || !physics::is_static(*mc)) {
					continue;
				}
				const auto* col_tc = transforms.find(col_eid);
				if (!col_tc) {
					continue;
				}
				const auto* shape = std::get_if<physics::box_shape>(&collisions[k].shape.value);
				if (!shape) {
					continue;
				}
				const physics::box_shape inflated{
					.size = shape->size + vec3<displacement>(inflation, inflation, inflation)
				};
				const bounding_box bb(*col_tc, inflated);
				if (const auto hit = narrow_phase_collision::segment_obb_first_hit(bb, current_pos, seg_end)) {
					if (!closest_hit || hit->distance < closest_hit->distance) {
						closest_hit = hit;
					}
				}
			}

			if (!closest_hit) {
				current_pos = current_pos + remaining;
				break;
			}

			const auto advance = std::max(closest_hit->distance - c.collision_skin, meters(0.f));
			current_pos = current_pos + seg_dir * advance;

			const auto leftover_length = remaining_length - closest_hit->distance;
			if (leftover_length <= meters(0.f)) {
				break;
			}
			const auto leftover = seg_dir * leftover_length;
			const auto along_normal = dot(leftover, closest_hit->normal);
			remaining = leftover - closest_hit->normal * along_normal;
		}

		cam_follow->position = current_pos;
	}

	std::erase_if(
		d.owners,
		[&cameras](const auto& entry) -> auto {
			return !cameras.find(entry.first);
		}
	);

	return {};
}
