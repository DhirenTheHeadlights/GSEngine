export module sandbox:player_state_broadcast;

import std;
import gse;

import :player_state;
import :role;

export namespace sandbox::player_state_broadcast {
	struct [[= gse::system_state<"PlayerStateBroadcast">{}]] data {
		std::unordered_map<gse::id, std::uint64_t> last_sent_step;
		gse::interval_timer<> report_timer{ gse::seconds(2.f) };
		std::uint32_t sent = 0;
		std::uint32_t unpossessed = 0;
		std::uint32_t repeat_step = 0;
		std::uint32_t missing_entry = 0;
		std::uint32_t missing_body = 0;
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
	if (!is_server(world_d)) {
		return {};
	}

	if (d.report_timer.tick()) {
		gse::log::println(
			gse::log::category::network,
			"player_state_broadcast: {} sent, {} unpossessed, {} repeat step, {} ring miss, {} body miss in the last window, ring {} entries, step {} observed {}",
			d.sent,
			d.unpossessed,
			d.repeat_step,
			d.missing_entry,
			d.missing_body,
			phys_d.rollback_ring.size(),
			phys_d.step_index,
			phys_d.observed_step
		);
		d.sent = 0;
		d.unpossessed = 0;
		d.repeat_step = 0;
		d.missing_entry = 0;
		d.missing_body = 0;
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
			++d.unpossessed;
			continue;
		}

		auto& last_sent = d.last_sent_step[controller_ids[i]];
		if (input->acked_step == last_sent) {
			++d.repeat_step;
			continue;
		}

		const auto& entry = phys_d.rollback_ring[input->acked_step % history];
		if (entry.step != input->acked_step) {
			++d.missing_entry;
			continue;
		}
		const auto body = std::ranges::find(entry.bodies, skeleton->proxy, &gse::physics::body_snapshot::owner);
		if (body == entry.bodies.end()) {
			++d.missing_body;
			continue;
		}

		++d.sent;

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
