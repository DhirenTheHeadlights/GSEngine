export module sandbox:player_reconcile;

import std;
import gse;

import :character_controller;
import :player_state;
import :player_sync;

export namespace sandbox::player_reconcile {
	struct correction_stats {
		std::uint32_t local_rollbacks = 0;
		std::uint32_t snaps = 0;
		gse::length local_max_error;
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

	struct [[= gse::system_state<"PlayerReconcile">{}]] data {
		bool server_offset_known = false;
		std::int64_t server_offset = 0;
		correction_stats stats;
		std::uint32_t reported_remote_starved = 0;
		gse::interval_timer<> report_timer{ gse::seconds(2.f) };
		std::size_t last_identity_hash = 0;
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
		gse::shared_view<player_sync::data> sync_d,
		gse::read<gse::skeleton_instance_component> skeletons,
		gse::read<gse::physics::collision_component> collisions,
		gse::read<gse::physics::collision_result_component> results,
		gse::read<gse::physics::motor_component> motors,
		gse::write<gse::physics::transform_component> transforms,
		gse::write<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

namespace sandbox::player_reconcile {
	constexpr auto correction_deadband = gse::meters(0.01f);
	constexpr std::uint64_t max_replay_steps = 24;
}

auto sandbox::player_reconcile::run(gse::context& ctx, data& d, const gse::channel_read<gse::network::received<player_state>> states_in, const gse::channel_write<gse::physics::rollback_request, gse::physics::impulse_request> physics_out, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::physics::data> phys_d, const gse::shared_view<character_controller::data> controller_d, const gse::shared_view<player_sync::data> sync_d, gse::read<gse::skeleton_instance_component> skeletons, gse::read<gse::physics::collision_component> collisions, gse::read<gse::physics::collision_result_component> results, gse::read<gse::physics::motor_component> motors, gse::write<gse::physics::transform_component> transforms, gse::write<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (d.report_timer.tick()) {
		const auto& s = d.stats;
		const auto remote_starved = sync_d.remote_starved - d.reported_remote_starved;
		d.reported_remote_starved = sync_d.remote_starved;
		if (s.local_rollbacks + remote_starved + s.snaps > 0 || d.server_offset_known) {
			gse::log::println(
				gse::log::category::network,
				"player_sync: {} local rollbacks (max error {}), {} remote tracks ({} starved frames), {} snaps, longest replay {} steps, ring {} entries at step {}, server step {} ack lag {} steps, local proxy at ({}, {}, {}) server says ({}, {}, {}); proxy shape {} resolve {} body {} mass {} colliding {} penetration {}",
				s.local_rollbacks,
				s.local_max_error,
				sync_d.remote_tracks,
				remote_starved,
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

	const auto local_character = world_d.local_controlled_entity;
	const auto* local_skeleton = local_character.exists() ? skeletons.find(local_character) : nullptr;
	const auto local_proxy = local_skeleton ? local_skeleton->proxy : gse::id{};
	{
		const auto identity = std::format(
			"player_sync: character {} skeleton {} proxy {} transform {} motion {} motor {}",
			local_character.number(),
			local_skeleton ? "present" : "missing",
			local_proxy.number(),
			transforms.find(local_proxy) ? "present" : "missing",
			motions.find(local_proxy) ? "present" : "missing",
			motors.find(local_proxy) ? "present" : "missing"
		);
		if (const auto hash = std::hash<std::string_view>{}(identity); hash != d.last_identity_hash) {
			d.last_identity_hash = hash;
			gse::log::println(gse::log::category::network, "{} controller {}", identity, controller_d.sequence);
		}
	}

	std::vector<player_state> pending = std::move(d.deferred);
	d.deferred.clear();
	for (const auto& received : states_in.of<gse::network::received<player_state>>()) {
		if (received.message.entity == local_proxy) {
			pending.push_back(received.message);
		}
	}

	const auto history = phys_d.rollback_ring.size();
	const auto ring_entry = [&](const std::uint64_t step) -> const gse::physics::step_snapshot* {
		if (history == 0 || step > phys_d.step_index || phys_d.step_index - step >= history) {
			return nullptr;
		}
		const auto& entry = phys_d.rollback_ring[step % history];
		return entry.step == step ? &entry : nullptr;
	};

	const auto controller_offset = static_cast<std::int64_t>(phys_d.step_index) - static_cast<std::int64_t>(controller_d.sequence);

	std::vector<gse::physics::body_override> overrides;
	std::uint64_t oldest = phys_d.step_index;

	for (const auto& state : pending) {
		const auto local_step = controller_offset + static_cast<std::int64_t>(state.input_sequence);
		d.server_offset = local_step - static_cast<std::int64_t>(state.server_step);
		d.server_offset_known = true;
		d.stats.last_server_step = state.server_step;
		d.stats.last_lag_steps = static_cast<std::int64_t>(phys_d.step_index) - local_step;

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
	if (steps == 0 || steps >= history || steps > max_replay_steps) {
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
