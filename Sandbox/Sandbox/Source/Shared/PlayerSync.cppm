export module sandbox:player_sync;

import std;
import gse;

import :character_controller;
import :player_state;
import :runtime_spawns;

export namespace sandbox::player_sync {
	struct correction_stats {
		std::uint32_t local_rollbacks = 0;
		std::uint32_t remote_rollbacks = 0;
		std::uint32_t snaps = 0;
		gse::length local_max_error;
		gse::length remote_max_error;
		std::uint32_t max_replay_steps = 0;
		gse::vec3<gse::position> last_server_position;
		gse::vec3<gse::position> last_local_position;
		int last_shape = -1;
		bool last_resolve = false;
		int last_body_kind = -1;
		gse::mass last_mass;
		bool last_colliding = false;
		gse::penetration last_penetration;
		std::uint64_t last_server_step = 0;
		std::int64_t last_lag_steps = 0;
	};

	struct [[= gse::system_state<"PlayerSync">{}]] data {
		bool server_offset_known = false;
		std::int64_t server_offset = 0;
		std::optional<gse::animation::locomotion_blend> clips;
		correction_stats stats;
		gse::interval_timer<> report_timer{ gse::seconds(2.f) };
		std::vector<player_state> deferred;
	};

	[[= gse::system_run<>{}]]
	[[= gse::runs_after<^^gse::physics::data>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::network::received<player_state>> states_in,
		gse::channel_write<gse::physics::rollback_request, gse::physics::impulse_request> physics_out,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::physics::data> phys_d,
		gse::shared_view<character_controller::data> controller_d,
		gse::shared_view<gse::asset::data> assets_d,
		gse::read<gse::skeleton_instance_component> skeletons,
		gse::read<gse::physics::collision_component> collisions,
		gse::read<gse::physics::collision_result_component> results,
		gse::write<gse::physics::transform_component> transforms,
		gse::write<gse::physics::motion_component> motions,
		gse::write<gse::physics::motor_component> motors,
		gse::write<gse::clip_player_component> players
	) -> gse::async::task<>;
}

namespace sandbox::player_sync {
	constexpr auto correction_deadband = gse::meters(0.01f);
}

auto sandbox::player_sync::run(gse::context& ctx, data& d, const gse::channel_read<gse::network::received<player_state>> states_in, const gse::channel_write<gse::physics::rollback_request, gse::physics::impulse_request> physics_out, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::physics::data> phys_d, const gse::shared_view<character_controller::data> controller_d, const gse::shared_view<gse::asset::data> assets_d, gse::read<gse::skeleton_instance_component> skeletons, gse::read<gse::physics::collision_component> collisions, gse::read<gse::physics::collision_result_component> results, gse::write<gse::physics::transform_component> transforms, gse::write<gse::physics::motion_component> motions, gse::write<gse::physics::motor_component> motors, gse::write<gse::clip_player_component> players) -> gse::async::task<> {
	if (d.report_timer.tick()) {
		const auto& s = d.stats;
		if (s.local_rollbacks + s.remote_rollbacks + s.snaps > 0 || d.server_offset_known) {
			gse::log::println(
				gse::log::category::network,
				"player_sync: {} local rollbacks (max error {}), {} remote rollbacks (max error {}), {} snaps, longest replay {} steps, ring {} entries at step {}, server step {} ack lag {} steps, local proxy at ({}, {}, {}) server says ({}, {}, {}); proxy shape {} resolve {} body {} mass {} colliding {} penetration {}",
				s.local_rollbacks,
				s.local_max_error,
				s.remote_rollbacks,
				s.remote_max_error,
				s.snaps,
				s.max_replay_steps,
				phys_d.rollback_ring.size(),
				phys_d.step_index,
				s.last_server_step,
				s.last_lag_steps,
				s.last_local_position.x(),
				s.last_local_position.y(),
				s.last_local_position.z(),
				s.last_server_position.x(),
				s.last_server_position.y(),
				s.last_server_position.z(),
				s.last_shape,
				s.last_resolve,
				s.last_body_kind,
				s.last_mass,
				s.last_colliding,
				s.last_penetration
			);
		}
		d.stats = {};
	}

	const auto states = states_in.of<gse::network::received<player_state>>();
	if (states.empty() && d.deferred.empty()) {
		return {};
	}
	std::vector<player_state> pending = std::move(d.deferred);
	d.deferred.clear();
	for (const auto& received : states) {
		pending.push_back(received.message);
	}

	const auto local_character = world_d.local_controlled_entity;
	const auto* local_skeleton = local_character.exists() ? skeletons.find(local_character) : nullptr;
	const auto local_proxy = local_skeleton ? local_skeleton->proxy : gse::id{};

	const auto history = phys_d.rollback_ring.size();
	const auto ring_entry = [&](const std::uint64_t step) -> const gse::physics::step_snapshot* {
		if (history == 0 || step > phys_d.step_index || phys_d.step_index - step >= history) {
			return nullptr;
		}
		const auto& entry = phys_d.rollback_ring[step % history];
		return entry.step == step ? &entry : nullptr;
	};

	const auto character_of = [&](const gse::id proxy) -> gse::id {
		const auto ids = skeletons.owner_ids();
		for (std::size_t i = 0; i < skeletons.size(); ++i) {
			if (skeletons[i].proxy == proxy) {
				return ids[i];
			}
		}
		return {};
	};

	const auto controller_offset = static_cast<std::int64_t>(phys_d.step_index) - static_cast<std::int64_t>(controller_d.sequence);
	if (!d.clips) {
		if (const auto resolved = character_clips(assets_d); resolved.idle.valid()) {
			d.clips = resolved;
		}
	}
	const character_controller::component defaults{};

	std::vector<gse::physics::body_override> overrides;
	std::uint64_t oldest = phys_d.step_index;

	for (const auto& state : pending) {
		const bool mine = state.entity == local_proxy;

		std::int64_t local_step = 0;
		if (mine) {
			local_step = controller_offset + static_cast<std::int64_t>(state.input_sequence);
			d.server_offset = local_step - static_cast<std::int64_t>(state.server_step);
			d.server_offset_known = true;
			d.stats.last_server_step = state.server_step;
			d.stats.last_lag_steps = static_cast<std::int64_t>(phys_d.step_index) - local_step;
		}
		else {
			if (!d.server_offset_known) {
				continue;
			}
			local_step = d.server_offset + static_cast<std::int64_t>(state.server_step);

			if (auto* motor = motors.find(state.entity)) {
				motor->velocity_drive_target = state.drive;
			}
			if (auto* tc = transforms.find(state.entity)) {
				tc->orientation = state.orientation;
			}
			if (auto* player = d.clips ? players.find(character_of(state.entity)) : nullptr) {
				const auto local = gse::inverse_rotate_vector(state.orientation, state.current_velocity);
				const gse::vec2f blend_input(-static_cast<float>(local.x() / defaults.run_speed), static_cast<float>(local.z() / defaults.run_speed));
				const auto speed = gse::magnitude(state.current_velocity);
				const float tier = speed > (defaults.walk_speed + defaults.run_speed) * 0.5f ? 1.f : 0.f;
				player->layer_count = gse::animation::blend_weights(*d.clips, blend_input, tier, player->layers);
				player->desired_speed = speed;
				player->playing = true;
			}
		}

		if (local_step < 0) {
			continue;
		}
		const auto step = static_cast<std::uint64_t>(local_step);
		if (step > phys_d.observed_step && step <= phys_d.step_index && phys_d.step_index - step < history) {
			d.deferred.push_back(state);
			continue;
		}

		auto* tc = transforms.find(state.entity);
		auto* mc = motions.find(state.entity);
		if (!tc || !mc) {
			continue;
		}
		if (mine) {
			d.stats.last_server_position = state.position;
			d.stats.last_local_position = tc->position;
			const auto* cc = collisions.find(state.entity);
			d.stats.last_shape = cc ? static_cast<int>(cc->shape.index()) : -1;
			d.stats.last_resolve = cc && cc->resolve_collisions;
			d.stats.last_body_kind = static_cast<int>(mc->body.index());
			d.stats.last_mass = gse::physics::is_dynamic(*mc) ? gse::physics::mass_of(*mc) : gse::kilograms(0.f);
			const auto* cr = results.find(state.entity);
			d.stats.last_colliding = cr && cr->colliding;
			d.stats.last_penetration = cr ? cr->penetration : gse::penetration{};
		}

		const auto* entry = ring_entry(step);
		const gse::physics::body_snapshot* body = nullptr;
		if (entry) {
			const auto it = std::ranges::find(entry->bodies, state.entity, &gse::physics::body_snapshot::owner);
			body = it == entry->bodies.end() ? nullptr : &*it;
		}
		if (!body) {
			tc->position = state.position;
			mc->current_velocity = state.current_velocity;
			physics_out.push<gse::physics::impulse_request>({
				.target = state.entity,
			});
			++d.stats.snaps;
			continue;
		}

		const gse::vec3<gse::length> delta = state.position - body->transform.position;
		const auto error = gse::magnitude(delta);
		if (error < correction_deadband) {
			continue;
		}
		if (!mine) {
			tc->position = state.position;
			mc->current_velocity = state.current_velocity;
			physics_out.push<gse::physics::impulse_request>({
				.target = state.entity,
			});
			++d.stats.remote_rollbacks;
			d.stats.remote_max_error = std::max(d.stats.remote_max_error, error);
			continue;
		}
		++d.stats.local_rollbacks;
		d.stats.local_max_error = std::max(d.stats.local_max_error, error);

		auto transform = body->transform;
		transform.position = state.position;
		auto motion = body->motion;
		motion.current_velocity = state.current_velocity;
		overrides.push_back({
			.step = step,
			.owner = state.entity,
			.transform = transform,
			.motion = motion,
		});
		oldest = std::min(oldest, step);
	}

	if (overrides.empty()) {
		return {};
	}

	const auto steps = phys_d.step_index - oldest;
	if (steps == 0 || steps >= history) {
		for (const auto& body : overrides) {
			if (auto* tc = transforms.find(body.owner)) {
				*tc = body.transform;
			}
			if (auto* mc = motions.find(body.owner)) {
				*mc = body.motion;
			}
			physics_out.push<gse::physics::impulse_request>({
				.target = body.owner,
			});
			++d.stats.snaps;
		}
		return {};
	}

	d.stats.max_replay_steps = std::max<std::uint32_t>(d.stats.max_replay_steps, static_cast<std::uint32_t>(steps));
	physics_out.push<gse::physics::rollback_request>({
		.steps = static_cast<std::uint32_t>(steps),
		.body_overrides = std::move(overrides),
	});

	return {};
}
