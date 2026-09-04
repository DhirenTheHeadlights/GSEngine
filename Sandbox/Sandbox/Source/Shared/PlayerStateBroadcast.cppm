export module sandbox:player_state_broadcast;

import std;
import gse;

import :player_state;

export namespace sandbox::player_state_broadcast {
	struct [[= gse::system_state<"PlayerStateBroadcast">{}]] data {
		std::unordered_map<gse::id, std::uint64_t> last_sent_step;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::physics::data> phys_d,
		gse::read<gse::player_controller> controllers,
		gse::read<gse::player_input> inputs,
		gse::read<gse::skeleton_instance_component> skeletons,
		gse::read<gse::physics::motor_component> motors,
		gse::channel_write<gse::network::send_request<player_state>> net_out
	) -> gse::async::task<>;
}

auto sandbox::player_state_broadcast::run(gse::context& ctx, data& d, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::physics::data> phys_d, gse::read<gse::player_controller> controllers, gse::read<gse::player_input> inputs, gse::read<gse::skeleton_instance_component> skeletons, gse::read<gse::physics::motor_component> motors, const gse::channel_write<gse::network::send_request<player_state>> net_out) -> gse::async::task<> {
	if (!world_d.networked || !world_d.authoritative) {
		return {};
	}

	const auto history = phys_d.rollback_ring.size();
	if (history == 0) {
		return {};
	}

	const auto controller_ids = controllers.owner_ids();
	for (std::size_t i = 0; i < controllers.size(); ++i) {
		const auto& pc = controllers[i];
		const auto* input = inputs.find(controller_ids[i]);
		const auto* skeleton = pc.controlled_entity_id.exists() ? skeletons.find(pc.controlled_entity_id) : nullptr;
		if (!input || !skeleton) {
			continue;
		}

		auto& last_sent = d.last_sent_step[controller_ids[i]];
		if (input->acked_step == last_sent) {
			continue;
		}

		const auto& entry = phys_d.rollback_ring[input->acked_step % history];
		if (entry.step != input->acked_step) {
			continue;
		}
		const auto body = std::ranges::find(entry.bodies, skeleton->proxy, &gse::physics::body_snapshot::owner);
		if (body == entry.bodies.end()) {
			continue;
		}

		last_sent = input->acked_step;
		const auto* motor = motors.find(skeleton->proxy);
		net_out.push<gse::network::send_request<player_state>>({
			.message = {
				.entity = skeleton->proxy,
				.server_step = input->acked_step,
				.input_sequence = input->acked_sequence,
				.position = body->transform.position,
				.current_velocity = body->motion.current_velocity,
				.orientation = body->transform.orientation,
				.drive = motor ? motor->velocity_drive_target : gse::vec3<gse::velocity>{},
			},
		});
	}

	return {};
}
