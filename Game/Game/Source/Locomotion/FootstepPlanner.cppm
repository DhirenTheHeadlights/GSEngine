export module gs:footstep_planner;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct footstep_planner {
		struct [[= gse::settings::category<"Locomotion">{}]] data {
			[[
				= gse::settings::describe<"Nominal forward step length at full input.">{}
			]]
			gse::displacement walk_step = gse::meters(0.16f);

			[[
				= gse::settings::describe<"Nominal forward step length when sprinting.">{}
			]]
			gse::displacement sprint_step = gse::meters(0.32f);

			[[
				= gse::settings::describe<"Maximum forward step distance.">{}
			]]
			gse::displacement max_forward_step = gse::meters(0.45f);

			[[
				= gse::settings::describe<"Maximum backward step distance.">{}
			]]
			gse::displacement max_backward_step = gse::meters(0.20f);

			[[
				= gse::settings::describe<"Maximum horizontal travel for one swing foot.">{}
			]]
			gse::displacement max_swing_reach = gse::meters(0.38f);

			[[
				= gse::settings::describe<"Forward capture-step clamp.">{}
			]]
			gse::displacement capture_step_limit = gse::meters(0.35f);

			[[
				= gse::settings::describe<"Scale applied to capture error before footstep planning.">{}
			]]
			float capture_step_gain = 0.65f;

			[[
				= gse::settings::describe<"Lateral capture-step clamp.">{}
			]]
			gse::displacement lateral_capture_limit = gse::meters(0.16f);

			[[
				= gse::settings::describe<"Foot center Y when planted on the ground.">{}
			]]
			gse::position foot_ground_y = gse::meters(0.025f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d
		) -> gse::async::task<>;
	};
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
		const plan& p
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

auto gs::locomotion::plan_may_update(const gait& g, const plan& p) -> bool {
	return g.current == phase::idle || g.current == phase::weight_shift ||
		(g.current == phase::swing && (!p.target_valid || p.swing_leg != g.swing_leg));
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
	return gse::vec3<gse::position>(swing_foot.x() + delta.x() * scale, d.foot_ground_y,
									swing_foot.z() + delta.z() * scale);
}

auto gs::locomotion::step_forward_with_capture(const gse::displacement nominal_forward, const gse::displacement capture_forward, const footstep_planner::data& d) -> gse::displacement {
	const auto conflict_threshold = d.walk_step * 0.5f;
	const bool forward_input_back_capture = nominal_forward > gse::meters(0.f) && capture_forward < -conflict_threshold;
	const bool backward_input_forward_capture = nominal_forward < gse::meters(0.f) && capture_forward > conflict_threshold;
	const auto requested_forward =
		forward_input_back_capture || backward_input_forward_capture ? capture_forward : nominal_forward + capture_forward;
	return std::clamp(requested_forward, -d.max_backward_step, d.max_forward_step);
}

auto gs::locomotion::plan_foot_target(const state& s, const gait& g, const intent& it, const skeleton_refs& r, const footstep_planner::data& d) -> gse::vec3<gse::position> {
	const auto step_length = it.sprint ? d.sprint_step : d.walk_step;
	const auto nominal_forward = step_length * it.forward;

	const auto capture_forward = std::clamp(
		s.capture_forward * d.capture_step_gain,
		-d.capture_step_limit,
		d.capture_step_limit
	);
	const auto capture_lateral = std::clamp(s.capture_right, -d.lateral_capture_limit, d.lateral_capture_limit);

	const auto step_forward = step_forward_with_capture(nominal_forward, capture_forward, d);
	const auto step_lateral = capture_lateral;

	const auto forward_xz = gse::normalize(gse::vec3f(s.pelvis_forward.x(), 0.f, s.pelvis_forward.z()));
	const auto right_xz = gse::normalize(gse::vec3f(s.pelvis_right.x(), 0.f, s.pelvis_right.z()));

	const auto hip_lateral = r.hip_offset_lateral * side_of(g.swing_leg);
	const auto target = s.support_center + forward_xz * step_forward + right_xz * (hip_lateral + step_lateral);

	return clamp_to_swing_reach(
		gse::vec3<gse::position>(target.x(), d.foot_ground_y, target.z()),
		swing_foot_position(s, g.swing_leg),
		d
	);
}

auto gs::locomotion::footstep_planner::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [refs, intents, states, gaits, plans] = co_await ctx.acquire_with(
				gse::read_v<skeleton_refs>,
				gse::read_v<intent>,
				gse::read_v<state>,
				gse::read_v<gait>,
				gse::write_v<plan>
			);

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

				const bool update = plan_may_update(*g, p);
				if (update) {
					p.foot_target_world = plan_foot_target(*s, *g, *it, *r, d);
					p.swing_leg = g->swing_leg;
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
		}

		co_await ctx.next_tick();
	}
}
