export module gs:player;

import std;
import gse;

import :locomotion_types;

export namespace gs::player {
	struct component {
		gse::id pelvis_id;
		gse::angle yaw = gse::degrees(-90.f);
		gse::angle pitch = gse::degrees(0.f);
		float mouse_sensitivity = 0.1f;
		bool jetpack_enabled = false;
		gse::length collision_radius = gse::meters(0.25f);
		gse::length collision_skin = gse::meters(0.05f);
		bool collide_with_geometry = true;
	};

	constexpr std::array<float, 4> camera_distance_levels_m = { 0.f, 2.f, 4.f, 6.f };

	struct bindings {
		gse::actions::handle shift;
		gse::actions::handle jump;
		gse::actions::handle jetpack_toggle;
		gse::id move_axis_id;
	};

}

export namespace gs::player {
	struct [[= gse::system_state<"Player">{}]] data {
		std::unordered_map<gse::id, bindings> bindings_by_owner;

		[[
			= gse::settings::describe<"Camera follow distance level (0=first person, 1-3=third person).">{},
			= gse::settings::range<0, 3>{}
		]]
		int camera_level = 0;

		[[
			= gse::settings::describe<"Smoothing factor on input axes per fixed step (0..1).">{}
		]]
		float input_smoothing = 0.20f;

		[[
			= gse::settings::describe<"Slerp rate per frame for pelvis facing toward yaw when grounded.">{}
		]]
		float facing_turn_rate = 0.03f;

		[[
			= gse::settings::describe<"Slerp rate per frame for pelvis facing toward yaw when airborne.">{}
		]]
		float facing_turn_rate_airborne = 0.01f;

		gse::interval_timer<float> input_log_timer{ gse::seconds(0.5f) };
	};

	[[= gse::system_run<>{}]]
	auto attach(
		gse::context& ctx,
		data& d,
		gse::read<component> players,
		gse::structural<gse::camera::follow_component> follows
	) -> gse::async::task<>;

	[[= gse::system_run<1>{}]]
	auto update(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::actions::data> as,
		gse::shared_view<gse::input::data> input_s,
		gse::shared_view<gse::camera::data> cam_s,
		gse::write<component> players,
		gse::write<gse::physics::transform_component> transforms,
		gse::write<gse::camera::follow_component> follows,
		gse::write<gs::locomotion::intent> intents,
		gse::read<gs::locomotion::state> states,
		gse::read<gs::locomotion::gait> gaits,
		gse::read<gse::physics::collision_component> collisions,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

namespace gs::player {
	struct intent_state {
		float forward = 0.f;
		float strafe = 0.f;
		float intensity = 0.f;
	};

	auto register_bindings(
		gse::context& ctx,
		bindings& b
	) -> void;
}

auto gs::player::register_bindings(gse::context& ctx, bindings& b) -> void {
	const auto w = gse::actions::add<"Player_Move_Forward">(ctx.channels, gse::key::w);
	const auto a = gse::actions::add<"Player_Move_Left">(ctx.channels, gse::key::a);
	const auto s_key = gse::actions::add<"Player_Move_Backward">(ctx.channels, gse::key::s);
	const auto d_key = gse::actions::add<"Player_Move_Right">(ctx.channels, gse::key::d);

	b.shift = gse::actions::add<"Player_Sprint">(ctx.channels, gse::key::left_shift);
	b.jump = gse::actions::add<"Player_Jump">(ctx.channels, gse::key::space);
	b.jetpack_toggle = gse::actions::add<"Toggle_Jetpack">(ctx.channels, gse::key::j);
	b.move_axis_id = gse::actions::bind_axis2(
		ctx.channels,
		gse::actions::pending_axis2_info{
			.left = a,
			.right = d_key,
			.back = s_key,
			.fwd = w,
			.scale = 1.f,
		},
		gse::trace_id<"Player_Move">()
	);
}

auto gs::player::attach(gse::context& ctx, data& d, gse::read<component> players, gse::structural<gse::camera::follow_component> follows) -> gse::async::task<> {
	for (const auto owner_id : ctx.drain_component_adds<component>()) {
		const auto* p = players.find(owner_id);
		if (!p) {
			continue;
		}
		register_bindings(ctx, d.bindings_by_owner[owner_id]);

		const gse::quat initial_orientation = gse::normalize(
			gse::quat(gse::vec3f(0.f, 1.f, 0.f), p->yaw) *
			gse::quat(gse::vec3f(1.f, 0.f, 0.f), p->pitch)
		);

		follows.add(
			owner_id,
			{
				.offset = gse::vec3<gse::length>(gse::meters(0.f)),
				.priority = 50,
				.blend_in_duration = gse::milliseconds(300),
				.active = true,
				.use_entity_position = false,
				.orientation = initial_orientation,
			}
		);
	}

	return {};
}

auto gs::player::update(gse::context& ctx, data& d, const gse::shared_view<gse::actions::data> as, const gse::shared_view<gse::input::data> input_s, const gse::shared_view<gse::camera::data> cam_s, gse::write<component> players, gse::write<gse::physics::transform_component> transforms, gse::write<gse::camera::follow_component> follows, gse::write<gs::locomotion::intent> intents, gse::read<gs::locomotion::state> states, gse::read<gs::locomotion::gait> gaits, gse::read<gse::physics::collision_component> collisions, gse::read<gse::physics::motion_component> motions) -> gse::async::task<> {
	const auto player_ids = players.owner_ids();

	const auto& cs = gse::actions::current_state(as);
	const auto& in = gse::input::current_state(input_s);
	const bool log_now = d.input_log_timer.tick();

	for (std::size_t i = 0; i < players.size(); ++i) {
		auto& p = players[i];
		const auto owner_id = player_ids[i];
		const auto binding_it = d.bindings_by_owner.find(owner_id);
		if (binding_it == d.bindings_by_owner.end()) {
			continue;
		}
		const auto& b = binding_it->second;

		const bool is_active_view = cam_s.active_controller_entity == owner_id;
		if (is_active_view && !cam_s.ui_focus) {
			const auto delta = in.mouse_delta();
			p.yaw -= gse::degrees(delta.x() * p.mouse_sensitivity);
			p.pitch -= gse::degrees(delta.y() * p.mouse_sensitivity);
			p.pitch = std::clamp(p.pitch, gse::degrees(-89.f), gse::degrees(89.f));

			const auto scroll = in.scroll_delta();
			if (scroll.y() > 0.f) {
				d.camera_level = std::max(0, d.camera_level - 1);
			}
			else if (scroll.y() < 0.f) {
				d.camera_level = std::min(static_cast<int>(camera_distance_levels_m.size()) - 1,
										  d.camera_level + 1);
			}
		}

		if (gse::actions::pressed(b.jetpack_toggle, cs, as)) {
			p.jetpack_enabled = !p.jetpack_enabled;
		}

		auto* pelvis_tc = transforms.find(p.pelvis_id);
		if (!pelvis_tc) {
			continue;
		}

		const auto move = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));
		const float forward_in = -move.y();
		const float strafe_in = move.x();
		const float move_magnitude = std::min(1.f, std::hypot(forward_in, strafe_in));
		constexpr float move_deadzone = 0.1f;

		if (auto* itn = intents.find(p.pelvis_id)) {
			const float k = d.input_smoothing;
			itn->forward += (move_magnitude - itn->forward) * k;
			itn->strafe += (0.f - itn->strafe) * k;
			itn->intensity += (move_magnitude - itn->intensity) * k;
			itn->sprint = gse::actions::held(b.shift, cs, as);
			itn->sprint_blend += ((itn->sprint ? 1.f : 0.f) - itn->sprint_blend) * 0.03f;
			itn->jump = false;
			const float camera_yaw = static_cast<float>(p.yaw);
			if (move_magnitude > move_deadzone) {
				itn->desired_yaw = gse::radians(std::atan2(
					forward_in * std::sin(camera_yaw) - strafe_in * std::cos(camera_yaw),
					forward_in * std::cos(camera_yaw) + strafe_in * std::sin(camera_yaw)
				));
			}
			else {
				itn->desired_yaw = gse::radians(std::remainder(camera_yaw, 2.f * std::numbers::pi_v<float>));
			}
			itn->has_heading = true;

			if (log_now) {
				gse::log::println(
					"player: owner={} intent=(fwd={:+.2f},strafe={:+.2f},int={:.2f},sprint={},jump={}) "
					"heading=({},{:+.2f}) yaw={:+.1f:deg} pitch={:+.1f:deg}",
					owner_id.number(),
					itn->forward,
					itn->strafe,
					itn->intensity,
					itn->sprint,
					itn->jump,
					itn->has_heading,
					itn->desired_yaw,
					p.yaw,
					p.pitch
				);
			}
		}

		if (auto* cam_follow = follows.find(owner_id)) {
			const auto level = static_cast<std::size_t>(
				std::clamp(
					d.camera_level,
					0,
					static_cast<int>(camera_distance_levels_m.size()) - 1
				)
			);
			const gse::length distance = gse::meters(camera_distance_levels_m[level]);
			const gse::quat camera_orientation = gse::normalize(
				gse::quat(gse::vec3f(0.f, 1.f, 0.f), p.yaw) *
				gse::quat(gse::vec3f(1.f, 0.f, 0.f), p.pitch)
			);
			const gse::vec3f camera_forward = gse::rotate_vector(camera_orientation, gse::vec3f(0.f, 0.f, -1.f));
			const auto target_pos = pelvis_tc->position;
			const auto inflation = p.collision_radius * 2.f;
			const auto for_each_static_box = [&](auto body) {
				const auto col_ids = collisions.owner_ids();
				for (std::size_t k = 0; k < collisions.size(); ++k) {
					const auto col_eid = col_ids[k];
					const auto* mc = motions.find(col_eid);
					if (!mc || !gse::physics::is_static(*mc)) {
						continue;
					}
					const auto* col_tc = transforms.find(col_eid);
					if (!col_tc) {
						continue;
					}
					const auto* shape = std::get_if<gse::physics::box_shape>(&collisions[k].shape.value);
					if (!shape) {
						continue;
					}
					const gse::physics::box_shape inflated{
						.size = shape->size + gse::vec3<gse::displacement>(inflation, inflation, inflation)
					};
					body(gse::bounding_box(*col_tc, inflated));
				}
			};

			gse::vec3<gse::displacement> spring_displacement = -camera_forward * distance;

			if (p.collide_with_geometry && distance > gse::meters(0.f)) {
				for_each_static_box([&](const gse::bounding_box& bb) {
					const auto q = gse::narrow_phase_collision::query_obb(bb, target_pos);
					if (q.signed_distance < gse::meters(0.f)) {
						const auto into_wall = -gse::dot(spring_displacement, q.normal);
						if (into_wall > gse::meters(0.f)) {
							spring_displacement += q.normal * into_wall;
						}
					}
				});
			}

			const auto desired_camera_pos = target_pos + spring_displacement;
			const auto displacement_mag = gse::magnitude(spring_displacement);
			float spring_factor = 1.f;
			if (p.collide_with_geometry && displacement_mag > gse::meters(1e-5f)) {
				for_each_static_box([&](const gse::bounding_box& bb) {
					if (const auto hit = gse::narrow_phase_collision::segment_obb_first_hit(bb, target_pos, desired_camera_pos)) {
						const auto safe = std::max(hit->distance - p.collision_skin, gse::meters(0.f));
						const float factor = safe / displacement_mag;
						if (factor < spring_factor) {
							spring_factor = factor;
						}
					}
				});
			}

			auto camera_pos = target_pos + spring_displacement * spring_factor;

			if (p.collide_with_geometry) {
				for_each_static_box([&](const gse::bounding_box& bb) {
					const auto result = gse::narrow_phase_collision::query_obb(bb, camera_pos);
					if (result.signed_distance < p.collision_skin) {
						const auto push = p.collision_skin - result.signed_distance;
						camera_pos = camera_pos + result.normal * push;
					}
				});
			}

			cam_follow->position = camera_pos;
			cam_follow->orientation = camera_orientation;
		}
	}

	std::erase_if(
		d.bindings_by_owner,
		[&players](const auto& entry) {
			return !players.find(entry.first);
		}
	);

	return {};
}
