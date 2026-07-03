export module gs:locomotion_smoke;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	enum class smoke_stage : std::uint8_t {
		locating_scene,
		warmup,
		running,
		resetting,
		done
	};

	struct smoke_config {
		[[= gse::at_least<0>{}]] int trials = 5;
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time duration = gse::seconds(20.f);
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time warmup = gse::seconds(1.5f);
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time stop_after = gse::seconds(13.f);
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time turn_after = gse::seconds(2.0f);
		gse::angle turn_by = gse::radians(1.57f);
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time sprint_after = gse::seconds(4.f);
		[[= gse::at_least<gse::seconds(0.f)>{}]] gse::time sprint_until = gse::seconds(8.f);
		[[= gse::within<-1.f, 1.f>{}]] float forward = 1.f;
	};
}

export namespace gs::locomotion::smoke_test {
	struct trial_metrics {
		gse::time elapsed = gse::seconds(0.f);
		gse::position min_pelvis_y = gse::meters(1000.f);
		gse::velocity max_speed = gse::meters_per_second(0.f);
		gse::displacement max_capture_forward = gse::meters(0.f);
		gse::displacement max_capture_right = gse::meters(0.f);
		gse::position max_foot_y = gse::meters(-1000.f);
		gse::position max_grounded_foot_y = gse::meters(-1000.f);
		gse::angle pitch_accum;
		int pitch_samples = 0;
		gse::vec3<gse::position> walk_start_position;
		gse::vec3<gse::position> walk_stop_position;
		bool walk_start_set = false;
		bool walk_stop_set = false;
		gse::vec3<gse::position> sprint_start_position;
		gse::vec3<gse::position> sprint_end_position;
		bool sprint_start_set = false;
		bool sprint_end_set = false;
		gse::vec3<gse::position> last_plant_position;
		bool last_plant_set = false;
		gse::displacement step_accum;
		int step_count = 0;
		gse::angle start_yaw;
		bool start_yaw_set = false;
		int plants = 0;
		int missed_plants = 0;
		int stale_targets = 0;
		phase last_phase = phase::idle;
		leg last_swing_leg = leg::left;
		bool counted_missed_plant = false;
		bool counted_stale_target = false;
		std::uint64_t state_hash = 1469598103934665603ull;
	};

	struct [[= gse::system_state<"Smoke">{}]] data {
		trial_metrics metrics;
		smoke_stage stage = smoke_stage::locating_scene;
		std::optional<gse::id> scene_id;
		gse::time stage_elapsed = gse::seconds(0.f);
		int trial = 0;
		int passed = 0;
		int failed = 0;
		int reset_ticks = 0;
		bool reset_activation_requested = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		const smoke_config& config,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<skeleton_refs> refs,
		gse::write<intent> intents,
		gse::read<state> states,
		gse::read<gait> gaits,
		gse::read<plan> plans
	) -> gse::async::task<>;
}

namespace gs::locomotion {
	auto sim_frame_dt() -> gse::time;

	auto find_scene_id(
		gse::shared_view<gse::world_system::data> world_d
	) -> std::optional<gse::id>;

	auto reset_metrics(
		smoke_test::trial_metrics& metrics
	) -> void;

	auto begin_trial(
		smoke_test::data& d,
		const smoke_config& config
	) -> void;

	auto smoke_foot_grounded(
		const state& s,
		leg which
	) -> bool;

	auto highest_foot_y(
		const state& s
	) -> gse::position;

	auto update_metrics(
		smoke_test::trial_metrics& metrics,
		const state& s,
		const gait& g,
		const plan& p,
		gse::time dt
	) -> void;

	auto write_smoke_intent(
		intent& it,
		float forward
	) -> void;

	auto finish_trial(
		gse::context& ctx,
		smoke_test::data& d,
		const smoke_config& config,
		const state& s,
		const gait& g
	) -> void;
}

auto gs::locomotion::sim_frame_dt() -> gse::time {
	return gse::system_clock::fixed_dt<gse::time>() * gse::system_clock::fixed_steps_this_frame();
}

auto gs::locomotion::find_scene_id(const gse::shared_view<gse::world_system::data> world_d) -> std::optional<gse::id> {
	for (const auto& scene_id : world_d.scene_ids) {
		if (scene_id.tag() == std::string_view("Sandbox")) {
			return scene_id;
		}
	}
	return std::nullopt;
}

auto gs::locomotion::reset_metrics(smoke_test::trial_metrics& metrics) -> void {
	metrics = {};
}

auto gs::locomotion::begin_trial(smoke_test::data& d, const smoke_config& config) -> void {
	reset_metrics(d.metrics);
	d.stage = smoke_stage::warmup;
	d.stage_elapsed = gse::seconds(0.f);
	d.reset_ticks = 0;
	d.reset_activation_requested = false;
	++d.trial;
	gse::log::println(
		"locomotion_smoke: trial={} begin duration={:.2f:s} warmup={:.2f:s} forward={:+.2f}",
		d.trial,
		config.duration,
		config.warmup,
		config.forward
	);
}

auto gs::locomotion::smoke_foot_grounded(const state& s, const leg which) -> bool {
	return which == leg::left ? s.foot_grounded_l : s.foot_grounded_r;
}

auto gs::locomotion::highest_foot_y(const state& s) -> gse::position {
	return s.foot_position_l.y() > s.foot_position_r.y() ? s.foot_position_l.y() : s.foot_position_r.y();
}

auto gs::locomotion::update_metrics(smoke_test::trial_metrics& metrics, const state& s, const gait& g, const plan& p, const gse::time dt) -> void {
	const auto fold = [&](auto unit_value) {
		metrics.state_hash ^= std::bit_cast<std::uint32_t>(unit_value);
		metrics.state_hash *= 1099511628211ull;
	};

	for (const auto& v : { s.pelvis_position, s.foot_position_l, s.foot_position_r }) {
		fold(v.x());
		fold(v.y());
		fold(v.z());
	}

	fold(s.pelvis_velocity.x());
	fold(s.pelvis_velocity.y());
	fold(s.pelvis_velocity.z());

	metrics.elapsed += dt;
	metrics.min_pelvis_y = std::min(metrics.min_pelvis_y, s.pelvis_position.y());
	metrics.max_speed = std::max(metrics.max_speed, s.horizontal_speed);
	metrics.pitch_accum += s.pelvis_pitch;
	++metrics.pitch_samples;
	metrics.max_capture_forward = std::max(metrics.max_capture_forward, gse::abs(s.capture_forward));
	metrics.max_capture_right = std::max(metrics.max_capture_right, gse::abs(s.capture_right));
	metrics.max_foot_y = std::max(metrics.max_foot_y, highest_foot_y(s));

	if (s.foot_grounded_l) {
		metrics.max_grounded_foot_y = std::max(metrics.max_grounded_foot_y, s.foot_position_l.y());
	}
	if (s.foot_grounded_r) {
		metrics.max_grounded_foot_y = std::max(metrics.max_grounded_foot_y, s.foot_position_r.y());
	}

	if (g.current == phase::plant && metrics.last_phase == phase::swing) {
		++metrics.plants;
		const auto& plant_foot = g.swing_leg == leg::left ? s.foot_position_l : s.foot_position_r;
		if (metrics.last_plant_set) {
			metrics.step_accum += gse::hypot(
				(plant_foot - metrics.last_plant_position).x(),
				(plant_foot - metrics.last_plant_position).z()
			);
			++metrics.step_count;
		}
		metrics.last_plant_position = plant_foot;
		metrics.last_plant_set = true;
	}

	if (g.current != metrics.last_phase || g.swing_leg != metrics.last_swing_leg) {
		metrics.counted_missed_plant = false;
		metrics.counted_stale_target = false;
	}

	const bool missed_plant = g.current == phase::plant && !smoke_foot_grounded(s, g.swing_leg) && g.phase_elapsed > g.phase_duration;
	if (missed_plant && !metrics.counted_missed_plant) {
		++metrics.missed_plants;
		metrics.counted_missed_plant = true;
	}
	if (!missed_plant) {
		metrics.counted_missed_plant = false;
	}

	const bool stale_target = p.target_valid && p.swing_leg != g.swing_leg;
	if (stale_target && !metrics.counted_stale_target) {
		++metrics.stale_targets;
		metrics.counted_stale_target = true;
	}
	if (!stale_target) {
		metrics.counted_stale_target = false;
	}

	metrics.last_phase = g.current;
	metrics.last_swing_leg = g.swing_leg;
}

auto gs::locomotion::write_smoke_intent(intent& it, const float forward) -> void {
	it.forward = forward;
	it.strafe = 0.f;
	it.intensity = std::min(std::abs(forward), 1.f);
	it.has_heading = false;
	it.sprint = false;
	it.jump = false;
}

auto gs::locomotion::finish_trial(gse::context& ctx, smoke_test::data& d, const smoke_config& config, const state& s, const gait& g) -> void {
	const bool passed = !g.fallen && d.metrics.elapsed >= config.duration;
	const std::string_view result = passed ? "passed" : "fallen";
	if (passed) {
		++d.passed;
	}
	else {
		++d.failed;
	}

	const auto mean_pitch = d.metrics.pitch_samples > 0 ? d.metrics.pitch_accum / static_cast<float>(d.metrics.pitch_samples) : gse::radians(0.f);
	const auto walk_end = d.metrics.walk_stop_set ? d.metrics.walk_stop_position : s.pelvis_position;
	const auto walk_time = d.metrics.walk_stop_set ? config.stop_after : d.metrics.elapsed;
	const auto walk_distance = d.metrics.walk_start_set
		? gse::hypot(
			  (walk_end - d.metrics.walk_start_position).x(),
			  (walk_end - d.metrics.walk_start_position).z()
		  )
		: gse::displacement{};
	const auto avg_speed = walk_time > gse::seconds(0.f) ? walk_distance / walk_time : gse::meters_per_second(0.f);
	const auto mean_step = d.metrics.step_count > 0
		? d.metrics.step_accum / static_cast<float>(d.metrics.step_count)
		: gse::displacement{};
	const auto sprint_window = config.sprint_until - config.sprint_after;
	const auto sprint_distance = d.metrics.sprint_start_set && d.metrics.sprint_end_set
		? gse::hypot(
			  (d.metrics.sprint_end_position - d.metrics.sprint_start_position).x(),
			  (d.metrics.sprint_end_position - d.metrics.sprint_start_position).z()
		  )
		: gse::displacement{};
	const auto sprint_speed = sprint_window > gse::seconds(0.f) ? sprint_distance / sprint_window : gse::meters_per_second(0.f);
	gse::log::println(
		"locomotion_smoke: trial={} result={} time={:.2f:s} plants={} min_pelvis_y={:.2f} "
		"avg_speed={:.2f} sprint_speed={:.2f} mean_step={:.2f} max_speed={:.2f} max_capture=(fwd={:.3f},right={:.3f}) max_foot_y={:.2f} heel_lift_y={:.3f} mean_pitch={:+.3f} "
		"missed_plants={} stale_targets={} phase={} state_hash={:016x} swing={}",
		d.trial,
		result,
		d.metrics.elapsed,
		d.metrics.plants,
		d.metrics.min_pelvis_y,
		avg_speed,
		sprint_speed,
		mean_step,
		d.metrics.max_speed,
		d.metrics.max_capture_forward,
		d.metrics.max_capture_right,
		d.metrics.max_foot_y,
		d.metrics.max_grounded_foot_y,
		mean_pitch,
		d.metrics.missed_plants,
		d.metrics.stale_targets,
		g.current, d.metrics.state_hash,
		g.swing_leg
	);

	if (d.trial >= config.trials) {
		d.stage = smoke_stage::done;
		gse::log::println("locomotion_smoke: complete trials={} passed={} failed={}", d.trial, d.passed, d.failed);
		gse::shutdown();
		return;
	}

	if (d.scene_id.has_value()) {
		ctx.channels.push<gse::deactivate_active_scene_request>({});
	}
	d.stage = smoke_stage::resetting;
	d.stage_elapsed = gse::seconds(0.f);
	d.reset_ticks = 0;
	d.reset_activation_requested = false;
}

auto gs::locomotion::smoke_test::run(gse::context& ctx, data& d, const smoke_config& config, const gse::shared_view<gse::world_system::data> world_d, gse::read<skeleton_refs> refs, gse::write<intent> intents, gse::read<state> states, gse::read<gait> gaits, gse::read<plan> plans) -> gse::async::task<> {
	const auto dt = sim_frame_dt();

	if (config.trials <= 0) {
		gse::log::println("locomotion_smoke: complete trials=0 passed=0 failed=0");
		gse::shutdown();
		return {};
	}

	if (!d.scene_id.has_value()) {
		d.scene_id = find_scene_id(world_d);
	}

	if (d.stage == smoke_stage::locating_scene) {
		if (d.scene_id.has_value() && world_d.active_scene != d.scene_id) {
			ctx.channels.push<gse::activate_scene_request>({
				.scene_id = *d.scene_id
			});
		}
		if (d.scene_id.has_value() && world_d.active_scene == d.scene_id) {
			d.stage = smoke_stage::resetting;
			d.stage_elapsed = gse::seconds(0.f);
			d.reset_ticks = 0;
			d.reset_activation_requested = false;
		}
	}

	if (d.stage == smoke_stage::resetting) {
		++d.reset_ticks;
		ctx.channels.push<gse::physics::reset_physics_request>({});
		if (d.scene_id.has_value() && world_d.active_scene == d.scene_id && !d.reset_activation_requested) {
			ctx.channels.push<gse::deactivate_active_scene_request>({});
		}
		if (d.reset_ticks >= 2 && d.scene_id.has_value() && world_d.active_scene != d.scene_id && !d.reset_activation_requested) {
			ctx.channels.push<gse::activate_scene_request>({
				.scene_id = *d.scene_id
			});
			d.reset_activation_requested = true;
		}
		if (d.reset_ticks >= 6 && d.reset_activation_requested && d.scene_id.has_value() && world_d.active_scene == d.scene_id) {
			begin_trial(d, config);
		}
	}

	std::optional<gse::id> owner;
	const auto owner_ids = refs.owner_ids();
	for (std::size_t i = 0; i < owner_ids.size(); ++i) {
		const auto candidate = owner_ids[i];
		if (intents.find(candidate) && states.find(candidate) && gaits.find(candidate) && plans.find(candidate)) {
			owner = candidate;
			break;
		}
	}

	if (owner.has_value()) {
		auto* it = intents.find(*owner);
		const auto* s = states.find(*owner);
		const auto* g = gaits.find(*owner);
		const auto* p = plans.find(*owner);
		if (it && s && g && p) {
			if (d.stage == smoke_stage::warmup || d.stage == smoke_stage::resetting) {
				write_smoke_intent(*it, 0.f);
			}
			if (d.stage == smoke_stage::warmup) {
				d.stage_elapsed += dt;
				if (d.stage_elapsed >= config.warmup && s->valid) {
					reset_metrics(d.metrics);
					d.stage = smoke_stage::running;
					d.stage_elapsed = gse::seconds(0.f);
					gse::log::println(
						"locomotion_smoke: trial={} run owner={} pelvis_y={:.2f} grounded=({},{})",
						d.trial,
						owner->number(),
						s->pelvis_position.y(),
						s->foot_grounded_l,
						s->foot_grounded_r
					);
				}
			}
			else if (d.stage == smoke_stage::running) {
				const bool stopped = config.stop_after > gse::seconds(0.f) && d.metrics.elapsed >= config.stop_after;
				write_smoke_intent(*it, stopped ? 0.f : config.forward);

				it->sprint = !stopped && config.sprint_until > config.sprint_after && d.metrics.elapsed >= config.sprint_after && d.metrics.elapsed < config.sprint_until;
				it->sprint_blend += ((it->sprint ? 1.f : 0.f) - it->sprint_blend) * 0.03f;

				if (!d.metrics.start_yaw_set && s->valid) {
					d.metrics.start_yaw = gse::radians(std::atan2(-s->pelvis_forward.x(), -s->pelvis_forward.z()));
					d.metrics.start_yaw_set = true;
				}

				const bool turning = config.turn_after > gse::seconds(0.f) && d.metrics.elapsed >= config.turn_after && d.metrics.start_yaw_set;

				if (turning) {
					it->desired_yaw = gse::radians(std::remainder(
						static_cast<float>(d.metrics.start_yaw + config.turn_by),
						2.f * std::numbers::pi_v<float>
					));
					it->has_heading = true;
				}

				update_metrics(d.metrics, *s, *g, *p, dt);
				
				if (!d.metrics.walk_start_set) {
					d.metrics.walk_start_position = s->pelvis_position;
					d.metrics.walk_start_set = true;
				}
				if (stopped && !d.metrics.walk_stop_set) {
					d.metrics.walk_stop_position = s->pelvis_position;
					d.metrics.walk_stop_set = true;
				}
				if (!d.metrics.sprint_start_set && d.metrics.elapsed >= config.sprint_after) {
					d.metrics.sprint_start_position = s->pelvis_position;
					d.metrics.sprint_start_set = true;
				}
				if (!d.metrics.sprint_end_set && d.metrics.elapsed >= config.sprint_until) {
					d.metrics.sprint_end_position = s->pelvis_position;
					d.metrics.sprint_end_set = true;
				}
				if (g->fallen || d.metrics.elapsed >= config.duration) {
					finish_trial(ctx, d, config, *s, *g);
				}
			}
		}
	}

	if (d.stage == smoke_stage::done) {
		return {};
	}

	return {};
}
