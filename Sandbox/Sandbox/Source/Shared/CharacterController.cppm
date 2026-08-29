export module sandbox:character_controller;

import std;
import gse;

import :orbit_camera;

export namespace sandbox::character_controller {
	struct component {
		gse::id proxy;
		gse::animation::locomotion_blend clips;
		bool possessed = false;
		gse::velocity run_speed = gse::meters_per_second(6.35f);
		gse::velocity walk_speed = gse::meters_per_second(3.3f);
		gse::acceleration ground_accelerate = gse::meters_per_second_squared(50.f);
		gse::acceleration ground_brake = gse::meters_per_second_squared(70.f);
		gse::time turn_response = gse::seconds(0.08f);
	};

	struct bindings {
		gse::actions::handle forward;
		gse::actions::handle back;
		gse::actions::handle left;
		gse::actions::handle right;
		gse::actions::handle walk;
		gse::id move_axis_id;
	};
}

export namespace sandbox::character_controller {
	struct [[= gse::system_state<"CharacterController">{}]] data {
		bindings binds;
		bool bound = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::actions::add_action_request, gse::actions::bind_axis2_request> actions_out,
		gse::shared_view<gse::actions::data> as,
		gse::read<component> characters,
		gse::read<orbit_camera::component> orbits,
		gse::read<gse::physics::motion_component> motions,
		gse::write<gse::clip_player_component> players,
		gse::write<gse::physics::motor_component> motors,
		gse::write<gse::physics::transform_component> transforms
	) -> gse::async::task<>;
}

auto sandbox::character_controller::run(gse::context& ctx, data& d, const gse::channel_write<gse::actions::add_action_request, gse::actions::bind_axis2_request> actions_out, const gse::shared_view<gse::actions::data> as, gse::read<component> characters, gse::read<orbit_camera::component> orbits, gse::read<gse::physics::motion_component> motions, gse::write<gse::clip_player_component> players, gse::write<gse::physics::motor_component> motors, gse::write<gse::physics::transform_component> transforms) -> gse::async::task<> {
	const auto& cs = gse::actions::current_state(as);
	const auto dt = gse::system_clock::dt();

	if (!d.bound) {
		d.binds.forward = gse::actions::add<"Character_Move_Forward">(actions_out, gse::key::w);
		d.binds.left = gse::actions::add<"Character_Move_Left">(actions_out, gse::key::a);
		d.binds.back = gse::actions::add<"Character_Move_Backward">(actions_out, gse::key::s);
		d.binds.right = gse::actions::add<"Character_Move_Right">(actions_out, gse::key::d);
		d.binds.walk = gse::actions::add<"Character_Walk">(actions_out, gse::key::left_shift);
		d.binds.move_axis_id = gse::actions::bind_axis2(
			actions_out,
			gse::actions::pending_axis2_info{
				.left = d.binds.left,
				.right = d.binds.right,
				.back = d.binds.back,
				.fwd = d.binds.forward,
				.scale = 1.f,
			},
			gse::trace_id<"Character_Move">()
		);
		d.bound = true;
		return {};
	}

	const auto character_ids = characters.owner_ids();
	for (std::size_t i = 0; i < characters.size(); ++i) {
		const auto owner = character_ids[i];
		const auto& c = characters[i];

		auto* motor = motors.find(c.proxy);
		auto* transform = transforms.find(c.proxy);
		if (!motor || !transform) {
			continue;
		}

		const auto* orbit = orbits.find(owner);
		const auto camera_yaw = orbit ? orbit->yaw : gse::degrees(0.f);
		const auto heading = gse::normalize(gse::quat(gse::vec3f(0.f, 1.f, 0.f), camera_yaw));

		const auto input = c.possessed
			? cs.axis2_v(static_cast<std::uint16_t>(d.binds.move_axis_id.number()))
			: gse::vec2f(0.f, 0.f);
		const auto move = gse::rotate_vector(heading, gse::vec3f(input.x(), 0.f, input.y()));
		const auto travel = gse::magnitude(move);

		const auto facing = gse::normalize(heading * gse::quat(gse::vec3f(0.f, 1.f, 0.f), gse::degrees(180.f)));

		const bool walking = c.possessed && gse::actions::held(d.binds.walk, cs, as);
		const auto speed = walking ? c.walk_speed : c.run_speed;

		const auto wish_dir = travel > 1e-4f ? move / travel : gse::vec3f(0.f);
		const auto wish_speed = std::min(travel, 1.f) * speed;

		const auto* proxy_motion = motions.find(c.proxy);
		const auto measured = proxy_motion ? proxy_motion->current_velocity : gse::vec3<gse::velocity>{};
		auto v = gse::vec3<gse::velocity>(measured.x(), gse::meters_per_second(0.f), measured.z());

		const auto desired = wish_dir * wish_speed;
		const auto delta = desired - v;
		const auto delta_speed = gse::magnitude(delta);

		const auto rate = gse::magnitude(desired) > gse::magnitude(v) ? c.ground_accelerate : c.ground_brake;
		const auto step = rate * dt;

		v = delta_speed <= step ? desired : v + delta * (step / delta_speed);

		motor->velocity_drive_target = v;

		const float blend = c.turn_response > gse::time{} ? std::min(dt / c.turn_response, 1.f) : 1.f;
		transform->orientation = gse::normalize(gse::slerp(transform->orientation, facing, blend));

		auto* player = players.find(owner);
		if (!player) {
			continue;
		}

		const gse::vec2f blend_input(input.x(), -input.y());
		player->layer_count = gse::animation::blend_weights(
			c.clips,
			blend_input,
			walking ? 0.f : 1.f,
			player->layers
		);
		player->desired_speed = gse::magnitude(v);
		player->playing = true;
	}

	return {};
}
