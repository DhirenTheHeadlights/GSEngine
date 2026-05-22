export module gs:leg_controller;

import std;
import gse;

import :controlled_joint;
import :leg_ik;
import :locomotion_types;

export namespace gs::locomotion {
	struct leg_controller {
		struct [[= gse::settings::category<"Legs">{}]] data {
			[[
				= gse::settings::describe<"Resting knee bend while standing (radians, negative = bent).">{}
			]]
			gse::angle stance_knee_rest = gse::radians(-0.05f);

			[[
				= gse::settings::describe<"Peak knee bend during swing midphase (radians, negative = lift foot).">{}
			]]
			gse::angle swing_knee_lift = gse::radians(-0.85f);

			[[
				= gse::settings::describe<"Lift height of swing foot at midphase.">{}
			]]
			gse::displacement swing_lift_height = gse::meters(0.12f);

			[[
				= gse::settings::describe<"Knee proportional gain on pelvis height error.">{}
			]]
			gse::inverse_length knee_height_gain = gse::per_meter(2.5f);

			[[
				= gse::settings::describe<"Hip proportional gain on body-frame forward lean.">{}
			]]
			gse::inverse_length hip_lean_gain = gse::per_meter(2.5f);

			[[
				= gse::settings::describe<"Stance hip extension while walking (per unit forward intent).">{}
			]]
			gse::angle stance_push_off = gse::radians(0.40f);

			[[
				= gse::settings::describe<"Clamp range on stance hip target.">{}
			]]
			gse::angle stance_hip_clamp = gse::radians(0.60f);

			[[
				= gse::settings::describe<"Clamp range on stance knee target.">{}
			]]
			gse::angle stance_knee_clamp = gse::radians(0.45f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	struct leg_joint_targets {
		gse::angle hip_l = gse::radians(0.f);
		gse::angle knee_l = gse::radians(0.f);
		gse::angle hip_r = gse::radians(0.f);
		gse::angle knee_r = gse::radians(0.f);
	};

	auto smooth_step(
		float t
	) -> float;
	auto compute_swing_foot(
		const gse::vec3<gse::position>& start,
		const gse::vec3<gse::position>& target,
		float t,
		gse::displacement lift_height
	) -> gse::vec3<gse::position>;
	auto compute_stance(
		const state& s,
		const intent& it,
		const skeleton_refs& r,
		const leg_controller::data& d
	) -> std::
		pair<gse::angle, gse::angle>;
	auto compute_swing(
		leg which,
		const state& s,
		const gait& g,
		const plan& p,
		const skeleton_refs& r,
		const leg_context& ctx,
		const leg_controller::data& d
	) -> std::
		pair<gse::angle, gse::angle>;
	auto write_targets(
		const skeleton_refs& r,
		const leg_joint_targets& targets,
		gse::write<controlled_joint_component>& ctrls,
		bool controllers_active
	) -> void;
}

auto gs::locomotion::smooth_step(const float t) -> float {
	const float tc = std::clamp(t, 0.f, 1.f);
	return tc * tc * (3.f - 2.f * tc);
}

auto gs::locomotion::compute_swing_foot(const gse::vec3<gse::position>& start, const gse::vec3<gse::position>& target, const float t, const gse::displacement lift_height) -> gse::vec3<gse::position> {
	const float lift = 4.f * t * (1.f - t);
	const auto base = gse::lerp(start, target, smooth_step(t));
	return gse::vec3<gse::position>(base.x(), base.y() + lift_height * lift, base.z());
}

auto gs::locomotion::compute_stance(const state& s, const intent& it, const skeleton_refs& r, const leg_controller::data& d) -> std::pair<gse::angle, gse::angle> {
	const auto height_error = r.pelvis_target_height - s.pelvis_position.y();
	const auto lift_error = std::max(height_error, gse::displacement{});
	const auto lean_back = s.lean_body.z();

	const auto knee_correction = gse::radians(1.f) * (d.knee_height_gain * lift_error);
	auto knee_target = d.stance_knee_rest + knee_correction;
	knee_target = std::clamp(knee_target, -d.stance_knee_clamp, gse::radians(0.f));

	const auto hip_correction = gse::radians(1.f) * (d.hip_lean_gain * lean_back);
	const auto push_off = d.stance_push_off * std::clamp(it.forward * it.intensity, -1.f, 1.f);
	auto hip_target = hip_correction + push_off;
	hip_target = std::clamp(hip_target, -d.stance_hip_clamp, d.stance_hip_clamp);

	return { hip_target, knee_target };
}

auto gs::locomotion::compute_swing(const leg which, const state& s, const gait& g, const plan& p, const skeleton_refs& r, const leg_context& ctx, const leg_controller::data& d) -> std::pair<gse::angle, gse::angle> {
	if (g.current == phase::idle) {
		return { gse::radians(0.f), d.stance_knee_rest };
	}

	if (g.current == phase::weight_shift) {
		const float t = phase_progress(g);
		const auto knee = d.stance_knee_rest + (d.swing_knee_lift * 0.15f - d.stance_knee_rest) * t;
		return { gse::radians(0.f), knee };
	}

	const auto start_foot =
		ctx.swing_initialized ? ctx.swing_start_foot : (which == leg::left ? s.foot_position_l : s.foot_position_r);
	const auto target_foot = p.target_valid ? p.foot_target_world : start_foot;
	const float t = phase_progress(g);
	const float trajectory_t = g.current == phase::plant ? 1.f : t;
	const auto lift = g.current == phase::plant ? gse::displacement{} : d.swing_lift_height;

	const auto desired_foot = compute_swing_foot(start_foot, target_foot, trajectory_t, lift);
	const auto hip_world = hip_world_position(s, r, which);
	const auto ik = solve_leg_ik(hip_world, desired_foot, s.pelvis_orientation, r.thigh_length, r.shin_length);

	return { ik.hip_pitch, ik.knee_bend };
}

auto gs::locomotion::write_targets(const skeleton_refs& r, const leg_joint_targets& targets, gse::write<controlled_joint_component>& ctrls, const bool controllers_active) -> void {
	auto apply = [&](const gse::id joint_id, const gse::angle target) {
		if (auto* cj = ctrls.find(joint_id)) {
			cj->enabled = controllers_active;
			cj->target_angle = target;
		}
	};
	apply(r.hip_l_joint_id, targets.hip_l);
	apply(r.knee_l_joint_id, targets.knee_l);
	apply(r.hip_r_joint_id, targets.hip_r);
	apply(r.knee_r_joint_id, targets.knee_r);
}

auto gs::locomotion::leg_controller::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [refs, intents, states, gaits, plans, contexts, ctrls] = co_await ctx.acquire_with(
				gse::read_v<skeleton_refs>,
				gse::read_v<intent>,
				gse::read_v<state>,
				gse::read_v<gait>,
				gse::read_v<plan>,
				gse::write_v<leg_context>,
				gse::write_v<controlled_joint_component>
			);

			const bool log_now = d.log_timer.tick();
			const auto owner_ids = contexts.owner_ids();
			for (std::size_t i = 0; i < contexts.size(); ++i) {
				auto& cctx = contexts[i];
				const auto owner = owner_ids[i];

				const auto* r = refs.find(owner);
				const auto* it = intents.find(owner);
				const auto* s = states.find(owner);
				const auto* g = gaits.find(owner);
				const auto* p = plans.find(owner);
				if (!r || !it || !s || !g || !p) {
					continue;
				}

				const bool entered_swing = g->current == phase::swing &&
					(cctx.last_phase != phase::swing || cctx.last_swing_leg != g->swing_leg);
				if (entered_swing) {
					const auto& current_foot = g->swing_leg == leg::left ? s->foot_position_l : s->foot_position_r;
					cctx.swing_start_foot = current_foot;
					cctx.swing_target_at_start = p->foot_target_world;
					cctx.swing_initialized = true;
				}
				if (g->current != phase::swing) {
					cctx.swing_initialized = false;
				}
				cctx.last_phase = g->current;
				cctx.last_swing_leg = g->swing_leg;

				const bool controllers_active = !g->fallen && s->valid;

				leg_joint_targets targets;
				if (controllers_active) {
					if (g->current == phase::idle) {
						const auto [hip, knee] = compute_stance(*s, *it, *r, d);
						targets.hip_l = hip;
						targets.knee_l = knee;
						targets.hip_r = hip;
						targets.knee_r = knee;
					}
					else {
						const leg stance_leg = other(g->swing_leg);
						const auto [stance_hip, stance_knee] = compute_stance(*s, *it, *r, d);
						const auto [swing_hip, swing_knee] = compute_swing(g->swing_leg, *s, *g, *p, *r, cctx, d);
						if (stance_leg == leg::left) {
							targets.hip_l = stance_hip;
							targets.knee_l = stance_knee;
							targets.hip_r = swing_hip;
							targets.knee_r = swing_knee;
						}
						else {
							targets.hip_r = stance_hip;
							targets.knee_r = stance_knee;
							targets.hip_l = swing_hip;
							targets.knee_l = swing_knee;
						}
					}
				}

				write_targets(*r, targets, ctrls, controllers_active);

				if (log_now) {
					gse::log::println(
						"leg_controller: owner={} phase={} swing={} fallen={} "
						"hips=({:+.3f},{:+.3f}) knees=({:+.3f},{:+.3f}) "
						"pelvis_y={:+.2f} target_y={:+.2f} lean_body=({:+.3f},{:+.3f})",
						owner.number(),
						g->current,
						g->swing_leg,
						g->fallen,
						targets.hip_l,
						targets.hip_r,
						targets.knee_l,
						targets.knee_r,
						s->pelvis_position.y(),
						r->pelvis_target_height,
						s->lean_body.x(),
						-s->lean_body.z()
					);
				}
			}
		}

		co_await ctx.next_tick();
	}
}
