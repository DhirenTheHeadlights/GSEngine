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
			gse::displacement walk_step = gse::meters(0.20f);

			[[
				= gse::settings::describe<"Nominal forward step length when sprinting.">{}
			]]
			gse::displacement sprint_step = gse::meters(0.32f);

			[[
				= gse::settings::describe<"Maximum forward step distance.">{}
			]]
			gse::displacement max_forward_step = gse::meters(0.35f);

			[[
				= gse::settings::describe<"Maximum backward step distance.">{}
			]]
			gse::displacement max_backward_step = gse::meters(0.20f);

			[[
				= gse::settings::describe<"Forward capture-step clamp.">{}
			]]
			gse::displacement capture_step_limit = gse::meters(0.25f);

			[[
				= gse::settings::describe<"Lateral capture-step clamp.">{}
			]]
			gse::displacement lateral_capture_limit = gse::meters(0.25f);

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
		const gait& g
	) -> bool;
}

auto gs::locomotion::plan_may_update(const gait& g) -> bool {
	return g.current == phase::idle || g.current == phase::weight_shift;
}

auto gs::locomotion::plan_foot_target(const state& s, const gait& g, const intent& it, const skeleton_refs& r, const footstep_planner::data& d) -> gse::vec3<gse::position> {
	const auto step_length = it.sprint ? d.sprint_step : d.walk_step;
	const auto nominal_forward = step_length * it.forward;

	const auto capture_forward = std::clamp(s.capture_forward, -d.capture_step_limit, d.capture_step_limit);
	const auto capture_lateral = std::clamp(s.capture_right, -d.lateral_capture_limit, d.lateral_capture_limit);

	const auto step_forward = std::clamp(nominal_forward + capture_forward, -d.max_backward_step, d.max_forward_step);
	const auto step_lateral = capture_lateral;

	const auto forward_xz = gse::normalize(gse::vec3f(s.pelvis_forward.x(), 0.f, s.pelvis_forward.z()));
	const auto right_xz = gse::normalize(gse::vec3f(s.pelvis_right.x(), 0.f, s.pelvis_right.z()));

	const auto hip_lateral = r.hip_offset_lateral * side_of(g.swing_leg);
	const auto target = s.support_center + forward_xz * step_forward + right_xz * (hip_lateral + step_lateral);

	return gse::vec3<gse::position>(target.x(), d.foot_ground_y, target.z());
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

				const bool update = plan_may_update(*g);
				if (update) {
					p.foot_target_world = plan_foot_target(*s, *g, *it, *r, d);
					p.target_valid = true;
					p.locked = false;
				}
				else {
					p.locked = true;
				}

				if (log_now) {
					gse::log::println(
						"footstep_planner: owner={} phase={} swing={} target=({:+.2f},{:+.2f},{:+.2f}) "
						"locked={} input_fwd={:+.2f} capture=(fwd={:+.3f},right={:+.3f})",
						owner.number(),
						g->current,
						g->swing_leg,
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
