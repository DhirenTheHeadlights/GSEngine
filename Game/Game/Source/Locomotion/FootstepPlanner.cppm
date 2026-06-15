export module gs:footstep_planner;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion::footstep_planner {
	struct [[= gse::system_state<"Locomotion">{}]] data {
		[[
			= gse::settings::describe<"Nominal forward step length at full input.">{}
		]]
		gse::displacement walk_step = gse::meters(0.36f);

		[[
			= gse::settings::describe<"Nominal forward step length when sprinting.">{}
		]]
		gse::displacement sprint_step = gse::meters(0.46f);

		[[
			= gse::settings::describe<"Maximum forward step distance.">{}
		]]
		gse::displacement max_forward_step = gse::meters(0.55f);

		[[
			= gse::settings::describe<"Maximum backward step distance.">{}
		]]
		gse::displacement max_backward_step = gse::meters(0.40f);

		[[
			= gse::settings::describe<"Maximum horizontal travel for one swing foot.">{}
		]]
		gse::displacement max_swing_reach = gse::meters(0.75f);

		[[
			= gse::settings::describe<"Forward capture-step clamp.">{}
		]]
		gse::displacement capture_step_limit = gse::meters(0.48f);

		[[
			= gse::settings::describe<"Scale applied to capture error before footstep planning.">{}
		]]
		float capture_step_gain = 1.0f;

		[[
			= gse::settings::describe<"Swing progress below which a committed target may still be refined once.">{}
		]]
		float swing_replan_progress = 0.5f;

		[[
			= gse::settings::describe<"Capture-point deviation from the committed sample that permits the one mid-swing refinement.">{}
		]]
		gse::displacement capture_replan_threshold = gse::meters(0.12f);

		[[
			= gse::settings::describe<"Maximum heading change applied to a single step's placement frame.">{}
		]]
		gse::angle turn_step_clamp = gse::radians(0.50f);

		[[
			= gse::settings::describe<"Lateral capture-step clamp.">{}
		]]
		gse::displacement lateral_capture_limit = gse::meters(0.22f);

		[[
			= gse::settings::describe<"Extra lateral spacing added to each foot target beyond the hip offset.">{}
		]]
		gse::displacement step_width_bias = gse::meters(0.05f);

		[[
			= gse::settings::describe<"Forward step bias per radian of forward trunk pitch (keeps support under a leaned trunk).">{}
		]]
		gse::displacement pitch_step_gain = gse::meters(0.55f);

		[[
			= gse::settings::describe<"Foot center Y when planted on the ground.">{}
		]]
		gse::position foot_ground_y = gse::meters(0.025f);

		gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		gse::read<skeleton_refs> refs,
		gse::read<intent> intents,
		gse::read<state> states,
		gse::read<gait> gaits,
		gse::write<plan> plans
	) -> gse::async::task<>;
}

namespace gs::locomotion {
	auto plan_foot_target(
		const state& s,
		const gait& g,
		const intent& it,
		const skeleton_refs& r,
		const footstep_planner::data& d
	) -> gse::vec3<gse::position>;

	auto plan_may_update(
		const gait& g,
		const plan& p,
		const state& s,
		const footstep_planner::data& d
	) -> bool;

	auto swing_foot_position(
		const state& s,
		leg swing_leg
	) -> const gse::vec3<gse::position>&;

	auto clamp_to_swing_reach(
		const gse::vec3<gse::position>& target,
		const gse::vec3<gse::position>& swing_foot,
		const footstep_planner::data& d
	) -> gse::vec3<gse::position>;

	auto step_forward_with_capture(
		gse::displacement nominal_forward,
		gse::displacement capture_forward,
		const footstep_planner::data& d
	) -> gse::displacement;
}

auto gs::locomotion::plan_may_update(const gait& g, const plan& p, const state& s, const footstep_planner::data& d) -> bool {
	if (g.current == phase::idle || g.current == phase::weight_shift || g.current == phase::recover) {
		return true;
	}
	if (g.current != phase::swing) {
		return false;
	}
	if (!p.target_valid || p.swing_leg != g.swing_leg) {
		return true;
	}
	if (p.refined || phase_progress(g) >= d.swing_replan_progress) {
		return false;
	}
	const auto deviation = gse::hypot(
		s.capture_forward - p.capture_forward_at_commit,
		s.capture_right - p.capture_right_at_commit
	);
	return deviation > d.capture_replan_threshold;
}

auto gs::locomotion::swing_foot_position(const state& s, const leg swing_leg) -> const gse::vec3<gse::position>& {
	return swing_leg == leg::left ? s.foot_position_l : s.foot_position_r;
}

auto gs::locomotion::clamp_to_swing_reach(const gse::vec3<gse::position>& target, const gse::vec3<gse::position>& swing_foot, const footstep_planner::data& d) -> gse::vec3<gse::position> {
	const auto delta = target - swing_foot;
	const auto horizontal_distance = gse::hypot(delta.x(), delta.z());
	if (horizontal_distance <= d.max_swing_reach || horizontal_distance <= gse::meters(0.001f)) {
		return gse::vec3<gse::position>(target.x(), d.foot_ground_y, target.z());
	}

	const float scale = d.max_swing_reach / horizontal_distance;
	return gse::vec3<gse::position>(swing_foot.x() + delta.x() * scale, d.foot_ground_y, swing_foot.z() + delta.z() * scale);
}

auto gs::locomotion::step_forward_with_capture(const gse::displacement nominal_forward, const gse::displacement capture_forward, const footstep_planner::data& d) -> gse::displacement {
	const auto conflict_threshold = d.walk_step * 0.5f;
	const bool forward_input_back_capture = nominal_forward > gse::meters(0.f) && capture_forward < -conflict_threshold;
	const bool backward_input_forward_capture = nominal_forward < gse::meters(0.f) && capture_forward > conflict_threshold;
	const auto requested_forward = forward_input_back_capture || backward_input_forward_capture ? capture_forward : nominal_forward + capture_forward;
	return std::clamp(requested_forward, -d.max_backward_step, d.max_forward_step);
}

auto gs::locomotion::plan_foot_target(const state& s, const gait& g, const intent& it, const skeleton_refs& r, const footstep_planner::data& d) -> gse::vec3<gse::position> {
	const float sprint_blend = std::clamp(it.sprint_blend, 0.f, 1.f);
	const auto step_length = d.walk_step + (d.sprint_step - d.walk_step) * sprint_blend;
	const auto nominal_forward = step_length * it.forward;

	const auto capture_forward = std::clamp(
		s.capture_forward * d.capture_step_gain,
		-d.capture_step_limit,
		d.capture_step_limit
	);
	const auto capture_lateral = std::clamp(s.capture_right, -d.lateral_capture_limit, d.lateral_capture_limit);

	const auto pitch_bias = d.pitch_step_gain * static_cast<float>(-s.pelvis_pitch);
	const auto step_forward = step_forward_with_capture(nominal_forward + pitch_bias, capture_forward, d);
	const auto step_lateral = capture_lateral;

	auto forward_xz = gse::normalize(gse::vec3f(s.pelvis_forward.x(), 0.f, s.pelvis_forward.z()));
	auto right_xz = gse::normalize(gse::vec3f(s.pelvis_right.x(), 0.f, s.pelvis_right.z()));

	if (it.has_heading) {
		const float turn = std::clamp(
			static_cast<float>(heading_error(s, it)),
			-static_cast<float>(d.turn_step_clamp),
			static_cast<float>(d.turn_step_clamp)
		);

		const float cos_turn = std::cos(turn);
		const float sin_turn = std::sin(turn);
		const auto rotate_about_y = [&](const gse::vec3f& v) {
			return gse::vec3f(v.x() * cos_turn + v.z() * sin_turn, 0.f, -v.x() * sin_turn + v.z() * cos_turn);
		};

		forward_xz = rotate_about_y(forward_xz);
		right_xz = rotate_about_y(right_xz);
	}

	const auto hip_lateral = (r.hip_offset_lateral + d.step_width_bias) * side_of(g.swing_leg);
	const auto lateral_recenter = dot(right_xz, s.pelvis_position - s.support_center);
	const auto target = s.support_center + forward_xz * step_forward + right_xz * (lateral_recenter + hip_lateral + step_lateral);

	return clamp_to_swing_reach(
		gse::vec3<gse::position>(target.x(), d.foot_ground_y, target.z()),
		swing_foot_position(s, g.swing_leg),
		d
	);
}

auto gs::locomotion::footstep_planner::run(data& d, gse::read<skeleton_refs> refs, gse::read<intent> intents, gse::read<state> states, gse::read<gait> gaits, gse::write<plan> plans) -> gse::async::task<> {
	const bool log_now = d.log_timer.tick();
	const auto owner_ids = plans.owner_ids();

	for (std::size_t i = 0; i < plans.size(); ++i) {
		auto& p = plans[i];
		const auto owner = owner_ids[i];

		const auto* r = refs.find(owner);
		const auto* it = intents.find(owner);
		const auto* s = states.find(owner);
		const auto* g = gaits.find(owner);
		if (!r || !it || !s || !g) {
			p.target_valid = false;
			p.locked = false;
			continue;
		}

		if (!s->valid || g->fallen) {
			p.target_valid = false;
			p.locked = false;
			continue;
		}

		const bool update = plan_may_update(*g, p, *s, d);
		if (update) {
			const bool swing_refine = g->current == phase::swing && p.target_valid && p.swing_leg == g->swing_leg;
			p.foot_target_world = plan_foot_target(*s, *g, *it, *r, d);
			p.swing_leg = g->swing_leg;
			p.capture_forward_at_commit = s->capture_forward;
			p.capture_right_at_commit = s->capture_right;
			p.refined = swing_refine;
			p.target_valid = true;
			p.locked = false;
		}
		else {
			p.locked = true;
		}

		if (log_now) {
			gse::log::println(
				"footstep_planner: owner={} phase={} swing={} planned={} target=({:+.2f},{:+.2f},{:+.2f}) "
				"locked={} input_fwd={:+.2f} capture=(fwd={:+.3f},right={:+.3f})",
				owner.number(),
				g->current,
				g->swing_leg,
				p.swing_leg,
				p.foot_target_world.x(),
				p.foot_target_world.y(),
				p.foot_target_world.z(),
				p.locked,
				it->forward,
				s->capture_forward,
				s->capture_right
			);
		}
	}

	return {};
}
