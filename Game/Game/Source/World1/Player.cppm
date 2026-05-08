export module gs:player;

import std;
import gse;

export namespace gs::player {
	struct component_data {
		gse::vec3<gse::position> initial_position;
		gse::velocity walk_speed = gse::miles_per_hour(20.f);
		gse::velocity sprint_speed = gse::miles_per_hour(40.f);
		gse::velocity jump_speed = gse::meters_per_second(7.f);
		gse::force jetpack_thrust = gse::newtons(1000.f);
		gse::force jetpack_side_force = gse::newtons(500.f);
		int boost_fuel_max = 1000;
		int boost_fuel = 1000;
		bool jetpack_enabled = false;
	};

	using component = gse::component<component_data>;

	struct bindings {
		gse::actions::handle shift;
		gse::actions::handle jump;
		gse::actions::handle jetpack_toggle;
		gse::actions::handle jetpack_thrust;
		gse::actions::handle jetpack_boost;
		gse::id move_axis_id;
		gse::id jetpack_move_axis_id;
	};

	struct system {
		struct state {
			std::unordered_map<gse::id, std::unique_ptr<bindings>> bindings_by_owner;
		};

		static auto run(
			gse::run_context& ctx,
			state& s,
			const gse::actions::system::state& as,
			const gse::camera::system::state& cam_s
		) -> gse::async::task<>;
	};
}

auto gs::player::system::run(gse::run_context& ctx, state& s, const gse::actions::system::state& as, const gse::camera::system::state& cam_s) -> gse::async::task<> {
	while (true) {
		{
			auto [players, motions, follows] = co_await ctx.acquire<
				gse::write<component>,
				gse::write<gse::physics::motion_component>,
				gse::write<gse::camera::follow_component>
			>();

			for (auto& p : players) {
				const auto owner_id = p.owner_id();
				auto& slot = s.bindings_by_owner[owner_id];
				if (slot) {
					continue;
				}

				slot = std::make_unique<bindings>();
				auto& b = *slot;

				const auto w = gse::actions::add<"Player_Move_Forward">(ctx.channels, gse::key::w);
				const auto a = gse::actions::add<"Player_Move_Left">(ctx.channels, gse::key::a);
				const auto s_key = gse::actions::add<"Player_Move_Backward">(ctx.channels, gse::key::s);
				const auto d = gse::actions::add<"Player_Move_Right">(ctx.channels, gse::key::d);

				b.shift = gse::actions::add<"Player_Sprint">(ctx.channels, gse::key::left_shift);
				b.jump = gse::actions::add<"Player_Jump">(ctx.channels, gse::key::space);
				b.move_axis_id = gse::actions::bind_axis2(
					ctx.channels,
					gse::actions::pending_axis2_info{
						.left = a,
						.right = d,
						.back = s_key,
						.fwd = w,
						.scale = 1.f,
					},
					gse::trace_id<"Player_Move">()
				);

				b.jetpack_toggle = gse::actions::add<"Toggle_Jetpack">(ctx.channels, gse::key::j);
				b.jetpack_thrust = gse::actions::add<"Jetpack_Thrust">(ctx.channels, gse::key::space);
				b.jetpack_boost = gse::actions::add<"Jetpack_Boost">(ctx.channels, gse::key::left_shift);

				const auto jw = gse::actions::add<"Jetpack_Move_Forward">(ctx.channels, gse::key::w);
				const auto ja = gse::actions::add<"Jetpack_Move_Left">(ctx.channels, gse::key::a);
				const auto js = gse::actions::add<"Jetpack_Move_Backward">(ctx.channels, gse::key::s);
				const auto jd = gse::actions::add<"Jetpack_Move_Right">(ctx.channels, gse::key::d);

				b.jetpack_move_axis_id = gse::actions::bind_axis2(
					ctx.channels,
					gse::actions::pending_axis2_info{
						.left = ja,
						.right = jd,
						.back = js,
						.fwd = jw,
						.scale = 1.f,
					},
					gse::trace_id<"Jetpack_Move">()
				);

				const gse::length height = gse::feet(6.0f);
				const gse::length width = gse::feet(3.0f);

				ctx.add_component<gse::physics::motion_component>(owner_id, {
					.current_position = p.initial_position,
					.mass = gse::pounds(180.f),
					.velocity_drive_target = {},
					.velocity_drive_active = true,
				});

				ctx.add_component<gse::physics::collision_component>(owner_id, {
					.bounding_box = {
						p.initial_position,
						{ width, height, width },
					},
				});

				ctx.add_component<gse::camera::follow_component>(owner_id, {
					.offset = gse::vec3<gse::length>(gse::meters(0.f)),
					.priority = 50,
					.blend_in_duration = gse::milliseconds(300),
					.active = true,
					.use_entity_position = false,
				});
			}

			const auto& cs = gse::actions::system::current_state(as);

			for (auto& p : players) {
				const auto owner_id = p.owner_id();
				const auto& b = *s.bindings_by_owner[owner_id];

				auto* motion = motions.find(owner_id);
				if (!motion) {
					continue;
				}

				const bool shift_held = gse::actions::held(b.shift, cs, as);
				const auto speed = shift_held ? p.sprint_speed : p.walk_speed;
				const auto v = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));

				if (v.x() != 0.f || v.y() != 0.f) {
					const auto dir = gse::camera::system::direction_relative_to_origin(
						cam_s,
						gse::vec3f(v.x(), 0.f, v.y())
					);
					const auto horizontal = gse::vec3f(dir.x(), 0.f, dir.z());
					const float len = gse::magnitude(horizontal);
					motion->velocity_drive_target = len > 1e-6f
						? speed * (horizontal / len)
						: gse::vec3<gse::velocity>{};
				}
				else {
					motion->velocity_drive_target = {};
				}

				if (gse::actions::pressed(b.jump, cs, as) && !motion->airborne) {
					motion->pending_impulse.y() = p.jump_speed * motion->mass;
				}

				if (gse::actions::pressed(b.jetpack_toggle, cs, as)) {
					p.jetpack_enabled = !p.jetpack_enabled;
				}

				if (p.jetpack_enabled && gse::actions::held(b.jetpack_thrust, cs, as)) {
					if (gse::actions::held(b.jetpack_boost, cs, as) && p.boost_fuel > 0) {
						p.boost_fuel -= 1;
					}
					else {
						p.boost_fuel = std::min(p.boost_fuel + 1, p.boost_fuel_max);
					}
				}

				if (auto* cam_follow = follows.find(owner_id)) {
					cam_follow->position = motion->current_position;
				}
			}

			std::erase_if(s.bindings_by_owner, [&players](const auto& entry) {
				return !players.find(entry.first);
			});
		}

		co_await ctx.next_tick();
	}
}
