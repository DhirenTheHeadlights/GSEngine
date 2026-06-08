export module gs:gait_scheduler;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct gait_config {
		gse::time weight_shift_duration = gse::seconds(0.15f);
		gse::time swing_duration = gse::seconds(0.65f);
		gse::time plant_duration = gse::seconds(0.12f);
		gse::time sprint_weight_shift_duration = gse::seconds(0.10f);
		gse::time sprint_swing_duration = gse::seconds(0.30f);
		gse::time sprint_plant_duration = gse::seconds(0.08f);
		float input_intensity_threshold = 0.05f;
		gse::displacement capture_step_threshold = gse::meters(0.06f);
		gse::position fall_pelvis_y_threshold = gse::meters(0.45f);
		gse::velocity fall_speed_threshold = gse::meters_per_second(3.0f);
		gse::displacement fall_capture_threshold = gse::meters(0.9f);
	};

	struct gait_scheduler {
		struct [[= gse::settings::category<"Gait">{}]] data {
			[[
				= gse::settings::describe<"Walking weight-shift phase duration.">{}
			]]
			gse::time weight_shift_duration = gse::seconds(0.25f);

			[[
				= gse::settings::describe<"Walking swing phase duration.">{}
			]]
			gse::time swing_duration = gse::seconds(0.65f);

			[[
				= gse::settings::describe<"Walking plant phase duration.">{}
			]]
			gse::time plant_duration = gse::seconds(0.24f);

			[[
				= gse::settings::describe<"Swing progress before contact may end the swing.">{}
			]]
			float swing_contact_progress = 0.90f;

			[[
				= gse::settings::describe<"Maximum horizontal foot-target error for accepting swing contact.">{}
			]]
			gse::displacement swing_target_tolerance = gse::meters(0.08f);

			[[
				= gse::settings::describe<"Maximum time a swing may wait for the foot target.">{}
			]]
			gse::time swing_timeout = gse::seconds(0.95f);

			[[
				= gse::settings::describe<"Swing progress before unstable posture may force planting.">{}
			]]
			float swing_drop_progress = 0.90f;

			[[
				= gse::settings::describe<"Forward capture limit for forcing a swing to plant.">{}
			]]
			gse::displacement swing_drop_capture_forward = gse::meters(0.38f);

			[[
				= gse::settings::describe<"Lateral capture limit for forcing a swing to plant.">{}
			]]
			gse::displacement swing_drop_capture_right = gse::meters(0.32f);

			[[
				= gse::settings::describe<"Capture-point magnitude that triggers a step from idle.">{}
			]]
			gse::displacement capture_step_threshold = gse::meters(0.06f);

			[[
				= gse::settings::describe<"Pelvis Y below which the character is declared fallen.">{}
			]]
			gse::position fall_pelvis_y_threshold = gse::meters(0.45f);

			[[
				= gse::settings::describe<"Minimum pelvis Y for releasing a swing from weight shift.">{}
			]]
			gse::position swing_pelvis_y_threshold = gse::meters(0.94f);

			[[
				= gse::settings::describe<"Minimum vertical pelvis velocity for releasing a swing from weight shift.">{}
			]]
			gse::velocity swing_vertical_velocity_limit = gse::meters_per_second(-0.20f);

			[[
				= gse::settings::describe<"Minimum pelvis Y for releasing a capture recovery swing.">{}
			]]
			gse::position recovery_swing_pelvis_y_threshold = gse::meters(0.82f);

			[[
				= gse::settings::describe<"Minimum vertical pelvis velocity for releasing a capture recovery swing.">{}
			]]
			gse::velocity recovery_swing_vertical_velocity_limit = gse::meters_per_second(-0.70f);

			[[
				= gse::settings::describe<"Forward capture limit for allowing a swing from weight shift.">{}
			]]
			gse::displacement swing_capture_forward_limit = gse::meters(0.28f);

			[[
				= gse::settings::describe<"Backward capture limit for allowing a swing from weight shift.">{}
			]]
			gse::displacement swing_capture_backward_limit = gse::meters(0.18f);

			[[
				= gse::settings::describe<"Lateral capture limit for allowing a swing from weight shift.">{}
			]]
			gse::displacement swing_capture_right_limit = gse::meters(0.24f);

			[[
				= gse::settings::describe<"Maximum time the recovery settle phase may hold support before forcing a step.">{}
			]]
			gse::time recovery_duration = gse::seconds(0.35f);

			[[
				= gse::settings::describe<"Forward capture magnitude at which recovery releases back to weight shift.">{}
			]]
			gse::displacement recovery_release_capture_forward = gse::meters(0.22f);

			[[
				= gse::settings::describe<"Lateral capture magnitude at which recovery releases back to weight shift.">{}
			]]
			gse::displacement recovery_release_capture_right = gse::meters(0.18f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			data& d,
			gse::read<skeleton_refs> refs,
			gse::read<intent> intents,
			gse::read<state> states,
			gse::read<plan> plans,
			gse::write<gait> gaits,
			gse::write<gse::physics::motion_component> motions
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	auto config_for(
		const gait_scheduler::data& d,
		const intent& it
	) -> gait_config;
	auto detect_fall(
		const state& s,
		const gait_config& cfg
	) -> bool;
	auto begin_phase(
		gait& g,
		phase p,
		gse::time duration,
		std::string_view reason,
		gse::id owner
	) -> void;
	auto foot_grounded(
		const state& s,
		leg which
	) -> bool;
	auto capture_safe_for_swing(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto capture_at_drop_threshold(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto recovery_release_ready(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto capture_demands_swing(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto capture_recovery_leg(
		const state& s,
		const gait_scheduler::data& d,
		leg fallback
	) -> leg;
	auto posture_safe_for_swing(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto posture_allows_recovery_swing(
		const state& s,
		const gait_scheduler::data& d
	) -> bool;
	auto swing_drop_requested(
		const state& s,
		const gait& g,
		const gait_scheduler::data& d
	) -> bool;
	auto foot_position(
		const state& s,
		leg which
	) -> const gse::vec3<gse::position>&;
	auto foot_target_reached(
		const state& s,
		const plan& p,
		leg which,
		const gait_scheduler::data& d
	) -> bool;
}

auto gs::locomotion::config_for(const gait_scheduler::data& d, const intent& it) -> gait_config {
	gait_config cfg{
		.weight_shift_duration = d.weight_shift_duration,
		.swing_duration = d.swing_duration,
		.plant_duration = d.plant_duration,
		.capture_step_threshold = d.capture_step_threshold,
		.fall_pelvis_y_threshold = d.fall_pelvis_y_threshold,
	};
	if (it.sprint) {
		cfg.weight_shift_duration = cfg.sprint_weight_shift_duration;
		cfg.swing_duration = cfg.sprint_swing_duration;
		cfg.plant_duration = cfg.sprint_plant_duration;
	}
	return cfg;
}

auto gs::locomotion::detect_fall(const state& s, const gait_config& cfg) -> bool {
	if (!s.valid) {
		return false;
	}
	const bool low_pelvis = s.pelvis_position.y() < cfg.fall_pelvis_y_threshold;
	const bool runaway_speed = s.horizontal_speed > cfg.fall_speed_threshold;
	const bool runaway_capture =
		s.capture_forward > cfg.fall_capture_threshold || s.capture_forward < -cfg.fall_capture_threshold;
	return low_pelvis || runaway_speed || runaway_capture;
}

auto gs::locomotion::begin_phase(gait& g, const phase p, const gse::time duration, const std::string_view reason, const gse::id owner) -> void {
	gse::log::println(
		"gait: owner={} {} -> {} duration={:.3f:s} swing={} reason={}",
		owner.number(),
		g.current,
		p,
		duration,
		g.swing_leg,
		reason
	);
	g.current = p;
	g.phase_elapsed = gse::seconds(0.f);
	g.phase_duration = duration;
}

auto gs::locomotion::foot_grounded(const state& s, const leg which) -> bool {
	return which == leg::left ? s.foot_grounded_l : s.foot_grounded_r;
}

auto gs::locomotion::capture_safe_for_swing(const state& s, const gait_scheduler::data& d) -> bool {
	return s.capture_forward <= d.swing_capture_forward_limit &&
		s.capture_forward >= -d.swing_capture_backward_limit &&
		gse::abs(s.capture_right) <= d.swing_capture_right_limit;
}

auto gs::locomotion::capture_at_drop_threshold(const state& s, const gait_scheduler::data& d) -> bool {
	return s.capture_forward > d.swing_drop_capture_forward ||
		gse::abs(s.capture_right) > d.swing_drop_capture_right;
}

auto gs::locomotion::recovery_release_ready(const state& s, const gait_scheduler::data& d) -> bool {
	return s.capture_forward <= d.recovery_release_capture_forward &&
		s.capture_forward >= -d.swing_capture_backward_limit &&
		gse::abs(s.capture_right) <= d.recovery_release_capture_right;
}

auto gs::locomotion::capture_demands_swing(const state& s, const gait_scheduler::data& d) -> bool {
	const bool forward_recovery = s.capture_forward > d.swing_capture_forward_limit;
	const bool backward_recovery = s.capture_forward < -d.swing_capture_backward_limit;
	const bool lateral_recovery = gse::abs(s.capture_right) > d.swing_capture_right_limit;
	return forward_recovery || backward_recovery || lateral_recovery;
}

auto gs::locomotion::capture_recovery_leg(const state& s, const gait_scheduler::data& d, const leg fallback) -> leg {
	if (gse::abs(s.capture_right) <= d.swing_capture_right_limit) {
		return fallback;
	}
	return s.capture_right > gse::meters(0.f) ? leg::right : leg::left;
}

auto gs::locomotion::posture_safe_for_swing(const state& s, const gait_scheduler::data& d) -> bool {
	return s.pelvis_position.y() >= d.swing_pelvis_y_threshold &&
		s.pelvis_velocity.y() >= d.swing_vertical_velocity_limit;
}

auto gs::locomotion::posture_allows_recovery_swing(const state& s, const gait_scheduler::data& d) -> bool {
	return s.pelvis_position.y() >= d.recovery_swing_pelvis_y_threshold &&
		s.pelvis_velocity.y() >= d.recovery_swing_vertical_velocity_limit;
}

auto gs::locomotion::swing_drop_requested(const state& s, const gait& g, const gait_scheduler::data& d) -> bool {
	const bool late_enough = phase_progress(g) >= d.swing_drop_progress;
	const bool posture_failed = !posture_safe_for_swing(s, d);
	const bool forward_runaway = s.capture_forward > d.swing_drop_capture_forward;
	const bool lateral_runaway = gse::abs(s.capture_right) > d.swing_drop_capture_right;
	const bool capture_runaway = forward_runaway || lateral_runaway;
	const bool falling = s.pelvis_velocity.y() < gse::meters_per_second(0.f);
	return (late_enough && (posture_failed || capture_runaway)) || (lateral_runaway && falling);
}

auto gs::locomotion::foot_position(const state& s, const leg which) -> const gse::vec3<gse::position>& {
	return which == leg::left ? s.foot_position_l : s.foot_position_r;
}

auto gs::locomotion::foot_target_reached(const state& s, const plan& p, const leg which, const gait_scheduler::data& d) -> bool {
	if (!p.target_valid) {
		return true;
	}
	const auto delta = foot_position(s, which) - p.foot_target_world;
	const auto horizontal_error = gse::hypot(delta.x(), delta.z());
	return horizontal_error <= d.swing_target_tolerance;
}

auto gs::locomotion::gait_scheduler::run(data& d, gse::read<skeleton_refs> refs, gse::read<intent> intents, gse::read<state> states, gse::read<plan> plans, gse::write<gait> gaits, gse::write<gse::physics::motion_component> motions) -> gse::async::task<> {
	const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
	const int fixed_steps = gse::system_clock::fixed_steps_this_frame();
	const auto frame_dt = step_dt * fixed_steps;
	const bool log_now = d.log_timer.tick();

	const auto owner_ids = gaits.owner_ids();
	for (std::size_t i = 0; i < gaits.size(); ++i) {
		auto& g = gaits[i];
		const auto owner = owner_ids[i];
		const auto* it = intents.find(owner);
		const auto* s = states.find(owner);
		const auto* p = plans.find(owner);
		if (!it || !s) {
			continue;
		}

		const auto cfg = config_for(d, *it);

		if (g.fallen) {
			continue;
		}

		if (detect_fall(*s, cfg)) {
			g.fallen = true;
			g.current = phase::idle;
			g.phase_elapsed = gse::seconds(0.f);
			g.phase_duration = gse::seconds(0.f);
			if (const auto* r = refs.find(owner)) {
				if (auto* m = motions.find(r->pelvis_id)) {
					if (auto* dyn = std::get_if<gse::physics::dynamic_body>(&m->body)) {
						dyn->update_orientation = true;
					}
				}
			}
			gse::log::println(
				"gait: owner={} ENTER FALLEN pelvis_y={:.2f} pelvis_vy={:+.2f} speed={:.2f} capture=(fwd={:+.3f},right={:+.3f})",
				owner.number(),
				s->pelvis_position.y(),
				s->pelvis_velocity.y(),
				s->horizontal_speed,
				s->capture_forward,
				s->capture_right
			);
			continue;
		}

		if (g.current != phase::idle) {
			g.phase_elapsed += frame_dt;
		}

		const bool input_wants_step = it->intensity > cfg.input_intensity_threshold;
		const bool capture_wants_step = s->valid &&
			(s->capture_forward > cfg.capture_step_threshold ||
			 s->capture_forward < -cfg.capture_step_threshold ||
			 gse::abs(s->capture_right) > cfg.capture_step_threshold);
		const bool wants_step = input_wants_step || capture_wants_step;

		switch (g.current) {
			case phase::idle:
				if (input_wants_step) {
					begin_phase(g, phase::weight_shift, cfg.weight_shift_duration, "input", owner);
				}
				break;

			case phase::weight_shift:
				if (g.phase_elapsed >= g.phase_duration && s->double_support) {
					const bool capture_safe = capture_safe_for_swing(*s, d);
					const bool recovery_step = capture_demands_swing(*s, d);
					const bool normal_swing = capture_safe && posture_safe_for_swing(*s, d);
					const bool recovery_swing = recovery_step && posture_allows_recovery_swing(*s, d);
					if (normal_swing || recovery_swing) {
						if (recovery_swing) {
							g.swing_leg = capture_recovery_leg(*s, d, g.swing_leg);
						}
						begin_phase(
							g,
							phase::swing,
							cfg.swing_duration,
							recovery_swing ? "capture" : "shift_done",
							owner
						);
					}
				}
				break;

			case phase::swing: {
				const bool swing_grounded = foot_grounded(*s, g.swing_leg);
				const bool target_reached = !p || foot_target_reached(*s, *p, g.swing_leg, d);
				const bool contact_allowed = phase_progress(g) >= d.swing_contact_progress;
				const bool timed_out = g.phase_elapsed >= d.swing_timeout;
				const bool drop_requested = swing_drop_requested(*s, g, d);
				if (swing_grounded && contact_allowed && target_reached) {
					begin_phase(g, phase::plant, cfg.plant_duration, "contact", owner);
				}
				else if (drop_requested) {
					begin_phase(g, phase::recover, d.recovery_duration, "drop", owner);
				}
				else if (timed_out) {
					if (swing_grounded) {
						begin_phase(g, phase::plant, cfg.plant_duration, "timed", owner);
					}
					else {
						begin_phase(g, phase::recover, d.recovery_duration, "timed_air", owner);
					}
				}
				break;
			}

			case phase::plant:
				if (g.phase_elapsed >= g.phase_duration) {
					const bool swing_grounded = foot_grounded(*s, g.swing_leg);
					const bool stance_grounded = foot_grounded(*s, other(g.swing_leg));
					const bool support_transferred = swing_grounded && !stance_grounded;
					const bool support_swing_safe = support_transferred && posture_allows_recovery_swing(*s, d);
					const leg next_swing_leg = other(g.swing_leg);
					if (!s->double_support && !support_swing_safe) {
						break;
					}
					const bool capture_too_hot = capture_at_drop_threshold(*s, d);
					if (capture_demands_swing(*s, d) && capture_too_hot && posture_allows_recovery_swing(*s, d)) {
						begin_phase(g, phase::recover, d.recovery_duration, "capture_hot", owner);
					}
					else if (capture_demands_swing(*s, d) && posture_allows_recovery_swing(*s, d)) {
						const leg recovery_leg = capture_recovery_leg(*s, d, next_swing_leg);
						g.swing_leg = recovery_leg == g.swing_leg ? next_swing_leg : recovery_leg;
						begin_phase(g, phase::swing, cfg.swing_duration, "capture", owner);
					}
					else if (capture_demands_swing(*s, d)) {
						break;
					}
					else if (!posture_safe_for_swing(*s, d)) {
						break;
					}
					else if (wants_step) {
						g.swing_leg = next_swing_leg;
						begin_phase(
							g,
							phase::weight_shift,
							cfg.weight_shift_duration,
							input_wants_step ? "input" : "capture",
							owner
						);
					}
					else {
						g.swing_leg = next_swing_leg;
						begin_phase(g, phase::idle, gse::seconds(0.f), "rest", owner);
					}
				}
				break;

			case phase::recover: {
				const bool posture_ok = posture_allows_recovery_swing(*s, d);
				const bool released = recovery_release_ready(*s, d) && posture_ok && s->double_support;
				const bool timed_out = g.phase_elapsed >= g.phase_duration;
				if (released) {
					g.swing_leg = capture_recovery_leg(*s, d, g.swing_leg);
					begin_phase(g, phase::weight_shift, cfg.weight_shift_duration, "recovered", owner);
				}
				else if (timed_out) {
					if (wants_step && posture_ok) {
						g.swing_leg = capture_recovery_leg(*s, d, g.swing_leg);
						begin_phase(g, phase::weight_shift, cfg.weight_shift_duration, "settle_timeout", owner);
					}
					else {
						begin_phase(g, phase::idle, gse::seconds(0.f), "settled", owner);
					}
				}
				break;
			}
		}

		if (log_now) {
			gse::log::println(
				"gait: owner={} phase={} swing={} t={:.2f:s}/{:.2f:s} fallen={} "
				"input=({:+.2f},{:+.2f},{:.2f}) capture=(fwd={:+.3f},right={:+.3f}) "
				"pelvis=(y={:+.2f},vy={:+.2f})",
				owner.number(),
				g.current,
				g.swing_leg,
				g.phase_elapsed,
				g.phase_duration,
				g.fallen,
				it->forward,
				it->strafe,
				it->intensity,
				s->capture_forward,
				s->capture_right,
				s->pelvis_position.y(),
				s->pelvis_velocity.y()
			);
		}
	}

	co_return;
}
