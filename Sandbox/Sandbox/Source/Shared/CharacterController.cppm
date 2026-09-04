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
		gse::time turn_response = gse::seconds(0.08f);
	};

	struct bindings {
		[[= gse::actions::bind<"Move Forward", gse::key::w>{}]]
		gse::actions::handle forward;

		[[= gse::actions::bind<"Move Backward", gse::key::s>{}]]
		gse::actions::handle back;

		[[= gse::actions::bind<"Move Left", gse::key::a>{}]]
		gse::actions::handle left;

		[[= gse::actions::bind<"Move Right", gse::key::d>{}]]
		gse::actions::handle right;

		[[= gse::actions::bind<"Walk", gse::key::left_shift>{}]]
		gse::actions::handle walk;

		[[= gse::actions::axis2<"Move", "left", "right", "back", "forward">{}]]
		gse::id move_axis_id;
	};
}

export namespace sandbox::character_controller {
	struct [[= gse::system_state<"CharacterController">{}]] data {
		[[= gse::actions::set{}]] bindings binds;
		[[= gse::shared]] std::uint64_t sequence = 0;
		gse::network::input_frame stash;
		bool has_stash = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::actions::data> as,
		gse::read<component> characters,
		gse::read<orbit_camera::component> orbits,
		gse::read<gse::player_controller> controllers,
		gse::read<gse::player_input> inputs,
		gse::write<gse::clip_player_component> players,
		gse::write<gse::physics::motor_component> motors,
		gse::write<gse::physics::transform_component> transforms,
		gse::channel_write<gse::network::send_request<gse::network::input_frame>> net_out
	) -> gse::async::task<>;
}

auto sandbox::character_controller::run(gse::context& ctx, data& d, const gse::shared_view<gse::actions::data> as, gse::read<component> characters, gse::read<orbit_camera::component> orbits, gse::read<gse::player_controller> controllers, gse::read<gse::player_input> inputs, gse::write<gse::clip_player_component> players, gse::write<gse::physics::motor_component> motors, gse::write<gse::physics::transform_component> transforms, const gse::channel_write<gse::network::send_request<gse::network::input_frame>> net_out) -> gse::async::task<> {
	const auto& local_state = gse::actions::current_state(as);
	const auto dt = gse::system_clock::dt();
	const int steps = std::max(gse::system_clock::fixed_steps_this_frame(), 0);
	const auto first_sequence = d.sequence + 1;
	d.sequence += static_cast<std::uint64_t>(steps);

	const auto remote_input_for = [&](const gse::id character) -> const gse::player_input* {
		const auto controller_ids = controllers.owner_ids();
		for (std::size_t i = 0; i < controllers.size(); ++i) {
			if (controllers[i].controlled_entity_id == character) {
				const auto* input = inputs.find(controller_ids[i]);
				return input && input->sequence > 0 ? input : nullptr;
			}
		}
		return nullptr;
	};

	const auto character_ids = characters.owner_ids();
	for (std::size_t i = 0; i < characters.size(); ++i) {
		const auto owner = character_ids[i];
		const auto& c = characters[i];

		auto* motor = motors.find(c.proxy);
		auto* transform = transforms.find(c.proxy);
		if (!motor || !transform) {
			continue;
		}

		const auto* remote = c.possessed ? nullptr : remote_input_for(owner);
		const bool driven = c.possessed || remote != nullptr;
		const auto& cs = remote ? remote->state : local_state;

		const auto* orbit = orbits.find(owner);
		const auto camera_yaw = remote ? remote->state.camera_yaw() : orbit ? orbit->yaw : gse::degrees(0.f);
		const auto heading = gse::normalize(gse::quat(gse::vec3f(0.f, 1.f, 0.f), camera_yaw));

		const auto input = driven
			? cs.axis2_v(static_cast<std::uint16_t>(d.binds.move_axis_id.number()))
			: gse::vec2f(0.f, 0.f);
		const auto move = gse::rotate_vector(heading, gse::vec3f(input.x(), 0.f, input.y()));
		const auto travel = gse::magnitude(move);

		const auto facing = gse::normalize(heading * gse::quat(gse::vec3f(0.f, 1.f, 0.f), gse::degrees(180.f)));

		const bool walking = driven && gse::actions::held(d.binds.walk, cs, as);
		const auto speed = walking ? c.walk_speed : c.run_speed;

		const auto wish_dir = travel > 1e-4f ? move / travel : gse::vec3f(0.f);
		const auto wish_speed = std::min(travel, 1.f) * speed;

		const auto v = wish_dir * wish_speed;
		motor->velocity_drive_target = v;

		const float blend = c.turn_response > gse::time{} ? std::min(dt / c.turn_response, 1.f) : 1.f;
		transform->orientation = gse::normalize(gse::slerp(transform->orientation, facing, blend));

		if (c.possessed) {
			for (int s = 0; d.has_stash && s < steps; ++s) {
				d.stash.input_sequence = static_cast<std::uint32_t>(first_sequence + static_cast<std::uint64_t>(s));
				net_out.push<gse::network::send_request<gse::network::input_frame>>({
					.message = d.stash,
				});
			}
			d.stash = gse::network::extract_input_frame(
				local_state,
				gse::actions::axis1_ids(as),
				gse::actions::axis2_ids(as),
				0,
				camera_yaw,
				orbit ? orbit->pitch : gse::degrees(0.f)
			);
			d.has_stash = true;
		}

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
