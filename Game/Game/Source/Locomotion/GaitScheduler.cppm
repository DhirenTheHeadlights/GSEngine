export module gs:gait_scheduler;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct gait_config {
		gse::time weight_shift_duration = gse::seconds(0.15f);
		gse::time swing_duration = gse::seconds(0.40f);
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
			[[= gse::settings::describe<"Walking weight-shift phase duration.">{}]] gse::time weight_shift_duration = gse::seconds(0.15f);

			[[= gse::settings::describe<"Walking swing phase duration.">{}]] gse::time swing_duration = gse::seconds(0.40f);

			[[= gse::settings::describe<"Walking plant phase duration.">{}]] gse::time plant_duration = gse::seconds(0.12f);

			[[= gse::settings::describe<"Capture-point magnitude that triggers a step from idle.">{}]] gse::displacement capture_step_threshold = gse::meters(0.06f);

			[[= gse::settings::describe<"Pelvis Y below which the character is declared fallen.">{}]] gse::position fall_pelvis_y_threshold = gse::meters(0.45f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d
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

auto gs::locomotion::gait_scheduler::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [refs, intents, states, gaits, motions] = co_await ctx.acquire_with(
				gse::read_v<skeleton_refs>,
				gse::read_v<intent>,
				gse::read_v<state>,
				gse::write_v<gait>,
				gse::write_v<gse::physics::motion_component>
			);

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
						"gait: owner={} ENTER FALLEN pelvis_y={:.2f} speed={:.2f} capture=(fwd={:+.3f},right={:+.3f})",
						owner.number(),
						s->pelvis_position.y(),
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
					 s->capture_forward < -cfg.capture_step_threshold);
				const bool wants_step = input_wants_step || capture_wants_step;

				switch (g.current) {
					case phase::idle:
						if (input_wants_step) {
							begin_phase(g, phase::weight_shift, cfg.weight_shift_duration, "input", owner);
						}
						break;

					case phase::weight_shift:
						if (g.phase_elapsed >= g.phase_duration) {
							begin_phase(g, phase::swing, cfg.swing_duration, "shift_done", owner);
						}
						break;

					case phase::swing: {
						const bool foot_l_swinging = g.swing_leg == leg::left;
						const bool swing_grounded = foot_l_swinging ? s->foot_grounded_l : s->foot_grounded_r;
						const bool min_swing_elapsed = g.phase_elapsed >= cfg.swing_duration * 0.5f;
						if ((min_swing_elapsed && swing_grounded) || g.phase_elapsed >= g.phase_duration) {
							begin_phase(
								g,
								phase::plant,
								cfg.plant_duration,
								swing_grounded ? "contact" : "timed",
								owner
							);
						}
						break;
					}

					case phase::plant:
						if (g.phase_elapsed >= g.phase_duration) {
							g.swing_leg = other(g.swing_leg);
							if (wants_step) {
								begin_phase(
									g,
									phase::weight_shift,
									cfg.weight_shift_duration,
									input_wants_step ? "input" : "capture",
									owner
								);
							}
							else {
								begin_phase(g, phase::idle, gse::seconds(0.f), "rest", owner);
							}
						}
						break;
				}

				if (log_now) {
					gse::log::println(
						"gait: owner={} phase={} swing={} t={:.2f:s}/{:.2f:s} fallen={} "
						"input=({:+.2f},{:+.2f},{:.2f}) capture=(fwd={:+.3f},right={:+.3f})",
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
						s->capture_right
					);
				}
			}
		}

		co_await ctx.next_tick();
	}
}
