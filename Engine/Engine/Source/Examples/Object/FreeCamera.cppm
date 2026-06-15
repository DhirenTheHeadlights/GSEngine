export module gse.examples:free_camera;

import std;

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

export namespace gse::free_camera {
	struct component {
		vec3<position> initial_position = vec3<position>(0.f, 0.f, 5.f);
		int priority = 10;
		velocity speed = meters_per_second(100.f);
		angle yaw = degrees(-90.f);
		angle pitch = degrees(0.f);
		float mouse_sensitivity = 0.1f;
		length collision_radius = meters(0.25f);
		length collision_skin = meters(0.05f);
		bool collide_with_geometry = true;
	};

	struct bindings {
		actions::handle forward;
		actions::handle left;
		actions::handle back;
		actions::handle right;
		actions::handle up;
		actions::handle down;
		id move_axis_id;
	};

}

export namespace gse::free_camera::system {
	struct [[= gse::system_state<"FreeCamera">{}]] data {
		std::unordered_map<id, bindings> bindings_by_owner;
	};

	[[= gse::system_run<>{}]]
	auto attach(
		context& ctx,
		data& d,
		read<component> cameras,
		structural<camera::follow_component> follows
	) -> async::task<>;

	[[= gse::system_run<1>{}]]
	auto update(
		context& ctx,
		data& d,
		shared_view<actions::data> as,
		shared_view<input::data> input_s,
		shared_view<camera::data> cam_s,
		write<component> cameras,
		write<camera::follow_component> follows,
		read<physics::transform_component> transforms,
		read<physics::collision_component> collisions,
		read<physics::motion_component> motions
	) -> async::task<>;
}

auto gse::free_camera::system::attach(context& ctx, data& d, read<component> cameras, structural<camera::follow_component> follows) -> async::task<> {
	for (const auto owner_id : ctx.drain_component_adds<component>()) {
		const auto* c = cameras.find(owner_id);
		if (!c) {
			continue;
		}
		auto& b = d.bindings_by_owner[owner_id];

		b.forward = actions::add<"FreeCamera_Move_Forward">(ctx.channels, key::w);
		b.left = actions::add<"FreeCamera_Move_Left">(ctx.channels, key::a);
		b.back = actions::add<"FreeCamera_Move_Backward">(ctx.channels, key::s);
		b.right = actions::add<"FreeCamera_Move_Right">(ctx.channels, key::d);
		b.up = actions::add<"FreeCamera_Move_Up">(ctx.channels, key::space);
		b.down = actions::add<"FreeCamera_Move_Down">(ctx.channels, key::left_control);

		b.move_axis_id = actions::bind_axis2(
			ctx.channels,
			actions::pending_axis2_info{
				.left = b.left,
				.right = b.right,
				.back = b.back,
				.fwd = b.forward,
				.scale = 1.f,
			},
			trace_id<"FreeCamera_Move">()
		);

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

auto gse::free_camera::system::update(context& ctx, data& d, const shared_view<actions::data> as, const shared_view<input::data> input_s, const shared_view<camera::data> cam_s, write<component> cameras, write<camera::follow_component> follows, read<physics::transform_component> transforms, read<physics::collision_component> collisions, read<physics::motion_component> motions) -> async::task<> {
	const auto camera_ids = cameras.owner_ids();

	const auto& cs = actions::current_state(as);
	const auto& in = input::current_state(input_s);

	for (std::size_t i = 0; i < cameras.size(); ++i) {
		auto& c = cameras[i];
		const auto owner_id = camera_ids[i];
		const auto binding_it = d.bindings_by_owner.find(owner_id);
		if (binding_it == d.bindings_by_owner.end()) {
			continue;
		}
		const auto& b = binding_it->second;

		auto* cam_follow = follows.find(owner_id);
		if (!cam_follow) {
			continue;
		}

		const bool is_active_view = cam_s.active_controller_entity == owner_id;
		const bool left_held = in.mouse_button_held(mouse_button::button_1);
		if (is_active_view && !cam_s.ui_focus && left_held) {
			const auto delta = in.mouse_delta();
			c.yaw -= degrees(delta.x() * c.mouse_sensitivity);
			c.pitch -= degrees(delta.y() * c.mouse_sensitivity);
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
		d.bindings_by_owner,
		[&cameras](const auto& entry) -> auto {
			return !cameras.find(entry.first);
		}
	);

	return {};
}
