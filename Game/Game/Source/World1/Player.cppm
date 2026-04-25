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
		gse::actions::button_channel shift;
		gse::actions::button_channel jump;
		gse::actions::button_channel jetpack_toggle;
		gse::actions::button_channel jetpack_thrust;
		gse::actions::button_channel jetpack_boost;
		gse::actions::axis2_channel move;
		gse::actions::axis2_channel jetpack_move;
	};

	struct state : gse::non_copyable {
		~state() override = default;

		std::unordered_map<gse::id, std::unique_ptr<bindings>> bindings_by_owner;
	};

	struct system {
		static auto update(
			gse::update_context& ctx,
			state& s
		) -> gse::async::task<>;
	};
}

auto gs::player::system::update(gse::update_context& ctx, state& s) -> gse::async::task<> {
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

		const auto w = gse::actions::add<"Player_Move_Forward">(gse::key::w);
		const auto a = gse::actions::add<"Player_Move_Left">(gse::key::a);
		const auto s_key = gse::actions::add<"Player_Move_Backward">(gse::key::s);
		const auto d = gse::actions::add<"Player_Move_Right">(gse::key::d);

		gse::actions::bind_button_channel<"Player_Sprint">(owner_id, gse::key::left_shift, b.shift);
		gse::actions::bind_button_channel<"Player_Jump">(owner_id, gse::key::space, b.jump);
		gse::actions::bind_axis2_channel(
			owner_id,
			gse::actions::pending_axis2_info{
				.left = a,
				.right = d,
				.back = s_key,
				.fwd = w,
				.scale = 1.f,
			},
			b.move,
			gse::trace_id<"Player_Move">()
		);

		gse::actions::bind_button_channel<"Toggle_Jetpack">(owner_id, gse::key::j, b.jetpack_toggle);
		gse::actions::bind_button_channel<"Jetpack_Thrust">(owner_id, gse::key::space, b.jetpack_thrust);
		gse::actions::bind_button_channel<"Jetpack_Boost">(owner_id, gse::key::left_shift, b.jetpack_boost);

		const auto jw = gse::actions::add<"Jetpack_Move_Forward">(gse::key::w);
		const auto ja = gse::actions::add<"Jetpack_Move_Left">(gse::key::a);
		const auto js = gse::actions::add<"Jetpack_Move_Backward">(gse::key::s);
		const auto jd = gse::actions::add<"Jetpack_Move_Right">(gse::key::d);

		gse::actions::bind_axis2_channel(
			owner_id,
			gse::actions::pending_axis2_info{
				.left = ja,
				.right = jd,
				.back = js,
				.fwd = jw,
				.scale = 1.f,
			},
			b.jetpack_move,
			gse::trace_id<"Jetpack_Move">()
		);

		const gse::length height = gse::feet(6.0f);
		const gse::length width = gse::feet(3.0f);

		ctx.defer_add<gse::physics::motion_component>(owner_id, gse::physics::motion_component_data{
			.current_position = p.initial_position,
			.mass = gse::pounds(180.f),
			.update_orientation = false,
			.velocity_drive_target = {},
			.velocity_drive_active = true,
		});

		ctx.defer_add<gse::physics::collision_component>(owner_id, gse::physics::collision_component_data{
			.bounding_box = {
				p.initial_position,
				{ width, height, width },
			},
		});

		ctx.defer_add<gse::camera::follow_component>(owner_id, gse::camera::follow_component_data{
			.offset = gse::vec3<gse::length>(gse::meters(0.f)),
			.priority = 50,
			.blend_in_duration = gse::milliseconds(300),
			.active = true,
			.use_entity_position = false,
		});
	}

	for (auto& p : players) {
		const auto owner_id = p.owner_id();
		const auto& b = *s.bindings_by_owner[owner_id];

		auto* motion = motions.find(owner_id);
		if (!motion) {
			continue;
		}

		const auto speed = b.shift.held ? p.sprint_speed : p.walk_speed;
		const auto v = b.move.value;

		if (v.x() != 0.f || v.y() != 0.f) {
			const auto dir = gse::camera_direction_relative_to_origin(
				gse::vec3f(v.x(), 0.f, v.y()),
				owner_id
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

		if (b.jump.pressed && !motion->airborne) {
			motion->pending_impulse.y() = p.jump_speed * motion->mass;
		}

		if (b.jetpack_toggle.pressed) {
			p.jetpack_enabled = !p.jetpack_enabled;
		}

		if (p.jetpack_enabled && b.jetpack_thrust.held) {
			if (b.jetpack_boost.held && p.boost_fuel > 0) {
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

	co_return;
}
