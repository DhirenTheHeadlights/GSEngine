export module sandbox:player_sync;

import std;
import gse;

import :character_controller;
import :characters;
import :player_state;

export namespace sandbox::player_sync {
	struct remote_sample {
		std::uint64_t server_step = 0;
		gse::vec3<gse::current_position> position;
		gse::vec3<gse::velocity> current_velocity;
		gse::quat orientation;
	};

	struct remote_track {
		std::vector<remote_sample> samples;
		std::optional<gse::physics::motion_component> previous_motion;
	};

	struct [[= gse::system_state<"PlayerSync">{}]] data {
		std::optional<gse::animation::locomotion_blend> clips;
		std::unordered_map<gse::id, remote_track> remotes;
		[[= gse::shared]] std::size_t remote_tracks = 0;
		[[= gse::shared]] std::uint32_t remote_starved = 0;
		double playback_step = 0.0;
		bool playback_valid = false;
		std::uint64_t newest_server_step = 0;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::network::received<player_state>> states_in,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::asset::data> assets_d,
		gse::read<gse::skeleton_instance_component> skeletons,
		gse::write<gse::physics::motion_component> motions,
		gse::write<gse::physics::motor_component> motors,
		gse::write<gse::physics::kinematic_target_component> kinematic_targets,
		gse::write<gse::clip_player_component> players,
		gse::structural<gse::physics::kinematic_target_component>
	) -> gse::async::task<>;
}

namespace sandbox::player_sync {
	constexpr double interpolation_delay_steps = 3.0;
	constexpr double playback_resync_steps = 12.0;
	constexpr double playback_catchup = 0.08;
	constexpr std::size_t max_remote_samples = 32;
}

auto sandbox::player_sync::run(gse::context& ctx, data& d, const gse::channel_read<gse::network::received<player_state>> states_in, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::asset::data> assets_d, gse::read<gse::skeleton_instance_component> skeletons, gse::write<gse::physics::motion_component> motions, gse::write<gse::physics::motor_component> motors, gse::write<gse::physics::kinematic_target_component> kinematic_targets, gse::write<gse::clip_player_component> players, gse::structural<gse::physics::kinematic_target_component>) -> gse::async::task<> {
	const auto local_character = world_d.local_controlled_entity;
	const auto* local_skeleton = local_character.exists() ? skeletons.find(local_character) : nullptr;
	const auto local_proxy = local_skeleton ? local_skeleton->proxy : gse::id{};

	const auto character_of = [&](const gse::id proxy) -> gse::id {
		const auto ids = skeletons.owner_ids();
		for (std::size_t i = 0; i < skeletons.size(); ++i) {
			if (skeletons[i].proxy == proxy) {
				return ids[i];
			}
		}
		return {};
	};

	if (!d.clips) {
		if (const auto resolved = character_clips(assets_d); resolved.idle.valid()) {
			d.clips = resolved;
		}
	}
	const character_controller::component defaults{};

	for (const auto& received : states_in.of<gse::network::received<player_state>>()) {
		const auto& state = received.message;
		d.newest_server_step = std::max(d.newest_server_step, state.server_step);
		if (state.entity == local_proxy) {
			continue;
		}

		auto& track = d.remotes[state.entity];
		const auto at = std::ranges::lower_bound(track.samples, state.server_step, std::ranges::less{}, &remote_sample::server_step);
		if (at == track.samples.end() || at->server_step != state.server_step) {
			track.samples.insert(
				at,
				remote_sample{
					.server_step = state.server_step,
					.position = state.position,
					.current_velocity = state.current_velocity,
					.orientation = state.orientation,
				}
			);
		}
		while (track.samples.size() > max_remote_samples) {
			track.samples.erase(track.samples.begin());
		}
	}

	if (d.newest_server_step != 0) {
		const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
		const auto target = static_cast<double>(d.newest_server_step) - interpolation_delay_steps;
		if (!d.playback_valid || std::abs(target - d.playback_step) > playback_resync_steps) {
			d.playback_valid = true;
			d.playback_step = target;
		}
		else {
			d.playback_step += gse::system_clock::dt<gse::time>() / step_dt;
			d.playback_step += (target - d.playback_step) * playback_catchup;
		}
	}

	std::erase_if(d.remotes, [&](const auto& entry) {
		return entry.second.previous_motion && !motions.find(entry.first);
	});

	for (auto& [entity, track] : d.remotes) {
		if (track.samples.empty()) {
			continue;
		}

		if (!track.previous_motion) {
			auto* mc = motions.find(entity);
			if (!mc) {
				continue;
			}
			auto* target = kinematic_targets.find(entity);
			if (!target) {
				ctx.add_component<gse::physics::kinematic_target_component>(
					entity,
					{
						.position = track.samples.back().position,
						.orientation = track.samples.back().orientation,
					}
				);
				continue;
			}
			track.previous_motion = *mc;
			mc->body = gse::physics::kinematic_body{};
			mc->current_velocity = {};
			mc->angular_velocity = {};
			if (auto* motor = motors.find(entity)) {
				motor->velocity_drive_target = {};
			}
		}

		auto* target = kinematic_targets.find(entity);
		if (!target) {
			continue;
		}

		const auto after = std::ranges::upper_bound(track.samples, d.playback_step, std::ranges::less{}, [](const remote_sample& s) { return static_cast<double>(s.server_step); });
		if (after == track.samples.begin() || after == track.samples.end()) {
			++d.remote_starved;
		}

		const auto& b = after == track.samples.end() ? track.samples.back() : *after;
		const auto& a = after == track.samples.begin() ? track.samples.front() : *std::prev(after);
		const auto span = static_cast<double>(b.server_step) - static_cast<double>(a.server_step);
		const float alpha = span > 0.0
			? static_cast<float>(std::clamp((d.playback_step - static_cast<double>(a.server_step)) / span, 0.0, 1.0))
			: 0.f;

		target->position = gse::lerp(a.position, b.position, alpha);
		target->orientation = gse::normalize(gse::slerp(a.orientation, b.orientation, alpha));

		const auto velocity = gse::lerp(a.current_velocity, b.current_velocity, alpha);
		while (track.samples.size() > 2 && static_cast<double>(track.samples[1].server_step) <= d.playback_step) {
			track.samples.erase(track.samples.begin());
		}

		if (auto* player = d.clips ? players.find(character_of(entity)) : nullptr) {
			const auto local = gse::inverse_rotate_vector(target->orientation, velocity);
			const gse::vec2f blend_input(-static_cast<float>(local.x() / defaults.run_speed), static_cast<float>(local.z() / defaults.run_speed));
			const auto speed = gse::magnitude(velocity);
			const float tier = speed > (defaults.walk_speed + defaults.run_speed) * 0.5f ? 1.f : 0.f;
			player->layer_count = gse::animation::blend_weights(*d.clips, blend_input, tier, player->layers);
			player->desired_speed = speed;
			player->playing = true;
		}
	}
	d.remote_tracks = d.remotes.size();

	return {};
}
