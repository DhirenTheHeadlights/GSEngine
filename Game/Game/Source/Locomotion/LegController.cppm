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
			gse::angle swing_knee_lift = gse::radians(-1.15f);

			[[
				= gse::settings::describe<"Lift height of swing foot at midphase.">{}
			]]
			gse::displacement swing_lift_height = gse::meters(0.26f);

			[[
				= gse::settings::describe<"Time used for swing-foot lift and descent.">{}
			]]
			gse::time swing_trajectory_duration = gse::seconds(0.48f);

			[[
				= gse::settings::describe<"Clamp range on stance hip target.">{}
			]]
			gse::angle stance_hip_clamp = gse::radians(0.60f);

			[[
				= gse::settings::describe<"Clamp range on stance knee target.">{}
			]]
			gse::angle stance_knee_clamp = gse::radians(0.45f);

			[[
				= gse::settings::describe<"Foot center Y used by stance IK.">{}
			]]
			gse::position stance_foot_ground_y = gse::meters(0.025f);

			[[
				= gse::settings::describe<"Maximum horizontal swing-foot travel while planting before contact.">{}
			]]
			gse::displacement plant_horizontal_reach = gse::meters(0.16f);

			[[
				= gse::settings::describe<"Distance below ground used as the active plant target before contact.">{}
			]]
			gse::displacement plant_foot_sink = gse::meters(0.08f);

			[[
				= gse::settings::describe<"Forward capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_forward_limit = gse::meters(0.28f);

			[[
				= gse::settings::describe<"Backward capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_backward_limit = gse::meters(0.18f);

			[[
				= gse::settings::describe<"Lateral capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_right_limit = gse::meters(0.24f);

			[[= gse::settings::describe<"Leg joint position gain.">{}]] float joint_gain = 30.f;

			[[= gse::settings::describe<"Leg joint velocity damping.">{}]] float joint_damping = 6.f;

			[[
				= gse::settings::describe<"Grounded foot anchor velocity gain.">{}
			]]
			gse::inverse_length foot_anchor_gain = gse::per_meter(4.f);

			[[
				= gse::settings::describe<"Maximum grounded foot anchor speed.">{}
			]]
			gse::velocity foot_anchor_max_speed = gse::meters_per_second(0.75f);

			[[
				= gse::settings::describe<"Maximum grounded foot anchor force.">{}
			]]
			gse::force foot_anchor_max_force = gse::newtons(450.f);

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
	auto swing_trajectory_progress(
		const gait& g,
		const leg_controller::data& d
	) -> float;
	auto compute_stance(
		leg which,
		const state& s,
		const skeleton_refs& r,
		const leg_context& ctx,
		const leg_controller::data& d
	) -> std::
		pair<gse::angle, gse::angle>;
	auto stance_foot_target(
		leg which,
		const leg_context& ctx,
		const leg_controller::data& d
	) -> gse::vec3<gse::position>;
	auto current_foot_position(
		leg which,
		const state& s
	) -> const gse::vec3<gse::position>&;
	auto planted_foot_position(
		leg which,
		leg_context& ctx
	) -> gse::vec3<gse::position>&;
	auto planted_foot_position(
		leg which,
		const leg_context& ctx
	) -> const gse::vec3<gse::position>&;
	auto foot_is_grounded(
		leg which,
		const state& s
	) -> bool;
	auto grounded_foot_position(
		const gse::vec3<gse::position>& foot,
		const leg_controller::data& d
	) -> gse::vec3<gse::position>;
	auto plant_contact_target(
		const gse::vec3<gse::position>& current,
		const gse::vec3<gse::position>& planned,
		const leg_controller::data& d
	) -> gse::vec3<gse::position>;
	auto clamp_foot_to_leg_reach(
		const gse::vec3<gse::position>& hip_world,
		const gse::vec3<gse::position>& target,
		const gse::quat& pelvis_orientation,
		gse::length thigh_length,
		gse::length shin_length
	) -> gse::vec3<gse::position>;
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
	auto weight_shift_ready(
		const state& s,
		const leg_controller::data& d
	) -> bool;
	auto write_targets(
		const skeleton_refs& r,
		const leg_joint_targets& targets,
		gse::write<controlled_joint_component>& ctrls,
		bool controllers_active,
		const leg_controller::data& d
	) -> void;
	auto foot_anchor_velocity(
		const gse::vec3<gse::position>& current,
		const gse::vec3<gse::position>& target,
		const leg_controller::data& d
	) -> gse::vec3<gse::velocity>;
	auto write_foot_motor(
		gse::id foot_id,
		const gse::vec3<gse::position>& current,
		const gse::vec3<gse::position>& target,
		bool active,
		gse::write<gse::physics::motor_component>& motors,
		const leg_controller::data& d
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

auto gs::locomotion::swing_trajectory_progress(const gait& g, const leg_controller::data& d) -> float {
	const auto duration = d.swing_trajectory_duration > gse::seconds(0.f) ? d.swing_trajectory_duration : g.phase_duration;
	if (duration <= gse::seconds(0.f)) {
		return 1.f;
	}
	return std::clamp(g.phase_elapsed / duration, 0.f, 1.f);
}

auto gs::locomotion::current_foot_position(const leg which, const state& s) -> const gse::vec3<gse::position>& {
	return which == leg::left ? s.foot_position_l : s.foot_position_r;
}

auto gs::locomotion::planted_foot_position(const leg which, leg_context& ctx) -> gse::vec3<gse::position>& {
	return which == leg::left ? ctx.planted_foot_l : ctx.planted_foot_r;
}

auto gs::locomotion::planted_foot_position(const leg which, const leg_context& ctx) -> const gse::vec3<gse::position>& {
	return which == leg::left ? ctx.planted_foot_l : ctx.planted_foot_r;
}

auto gs::locomotion::foot_is_grounded(const leg which, const state& s) -> bool {
	return which == leg::left ? s.foot_grounded_l : s.foot_grounded_r;
}

auto gs::locomotion::grounded_foot_position(const gse::vec3<gse::position>& foot, const leg_controller::data& d) -> gse::vec3<gse::position> {
	return gse::vec3<gse::position>(foot.x(), d.stance_foot_ground_y, foot.z());
}

auto gs::locomotion::plant_contact_target(const gse::vec3<gse::position>& current, const gse::vec3<gse::position>& planned, const leg_controller::data& d) -> gse::vec3<gse::position> {
	const auto grounded_current = grounded_foot_position(current, d);
	const auto grounded_planned = grounded_foot_position(planned, d);
	const auto delta = grounded_planned - grounded_current;
	const auto horizontal_distance = gse::hypot(delta.x(), delta.z());
	const auto plant_y = d.stance_foot_ground_y - d.plant_foot_sink;
	if (horizontal_distance <= d.plant_horizontal_reach || horizontal_distance <= gse::meters(0.001f)) {
		return gse::vec3<gse::position>(grounded_planned.x(), plant_y, grounded_planned.z());
	}

	const float scale = d.plant_horizontal_reach / horizontal_distance;
	return gse::vec3<gse::position>(
		grounded_current.x() + delta.x() * scale,
		plant_y,
		grounded_current.z() + delta.z() * scale
	);
}

auto gs::locomotion::clamp_foot_to_leg_reach(const gse::vec3<gse::position>& hip_world, const gse::vec3<gse::position>& target, const gse::quat& pelvis_orientation, const gse::length thigh_length, const gse::length shin_length) -> gse::vec3<gse::position> {
	const auto rel_world = target - hip_world;
	const auto rel_body = gse::inverse_rotate_vector(pelvis_orientation, rel_world);
	const gse::length sagittal = -rel_body.z();
	const gse::length drop = -rel_body.y();
	const auto max_reach = thigh_length + shin_length - gse::meters(0.03f);
	const auto drop_abs = gse::abs(drop);

	if (drop_abs >= max_reach) {
		const auto clamped_drop = std::clamp(drop, -max_reach, max_reach);
		const auto clamped_rel_body = gse::vec3<gse::displacement>(
			rel_body.x(),
			-clamped_drop,
			gse::meters(0.f)
		);
		return hip_world + gse::rotate_vector(pelvis_orientation, clamped_rel_body);
	}

	const auto max_sagittal = gse::sqrt(max_reach * max_reach - drop * drop);
	if (gse::abs(sagittal) <= max_sagittal) {
		return target;
	}

	const auto clamped_sagittal = std::clamp<gse::length>(sagittal, -max_sagittal, max_sagittal);
	const auto clamped_rel_body = gse::vec3<gse::displacement>(
		rel_body.x(),
		rel_body.y(),
		-clamped_sagittal
	);
	return hip_world + gse::rotate_vector(pelvis_orientation, clamped_rel_body);
}

auto gs::locomotion::stance_foot_target(const leg which, const leg_context& ctx, const leg_controller::data& d) -> gse::vec3<gse::position> {
	return grounded_foot_position(planted_foot_position(which, ctx), d);
}

auto gs::locomotion::compute_stance(const leg which, const state& s, const skeleton_refs& r, const leg_context& ctx, const leg_controller::data& d) -> std::pair<gse::angle, gse::angle> {
	auto support_state = s;
	support_state.pelvis_position.y() = std::max(
		support_state.pelvis_position.y(),
		r.pelvis_target_height
	);

	const auto hip_world = hip_world_position(support_state, r, which);
	const auto foot_target = stance_foot_target(which, ctx, d);
	const auto ik = solve_leg_ik(
		hip_world,
		foot_target,
		s.pelvis_orientation,
		r.thigh_length,
		r.shin_length
	);

	const auto hip_target = std::clamp(ik.hip_pitch, -d.stance_hip_clamp, d.stance_hip_clamp);
	const auto knee_target = std::clamp(ik.knee_bend, -d.stance_knee_clamp, gse::radians(0.f));

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
	auto target_foot = p.target_valid ? p.foot_target_world : start_foot;
	if (g.current == phase::plant) {
		target_foot = foot_is_grounded(which, s) && p.target_valid ?
			grounded_foot_position(p.foot_target_world, d) :
			(p.target_valid ?
				 plant_contact_target(current_foot_position(which, s), p.foot_target_world, d) :
				 grounded_foot_position(current_foot_position(which, s), d));
	}
	const float t = swing_trajectory_progress(g, d);
	const float trajectory_t = g.current == phase::plant ? 1.f : t;
	const auto lift = g.current == phase::plant ? gse::displacement{} : d.swing_lift_height;

	const auto hip_world = hip_world_position(s, r, which);
	const auto desired_foot = clamp_foot_to_leg_reach(
		hip_world,
		compute_swing_foot(start_foot, target_foot, trajectory_t, lift),
		s.pelvis_orientation,
		r.thigh_length,
		r.shin_length
	);
	const auto ik = solve_leg_ik(
		hip_world,
		desired_foot,
		s.pelvis_orientation,
		r.thigh_length,
		r.shin_length
	);

	return { ik.hip_pitch, ik.knee_bend };
}

auto gs::locomotion::weight_shift_ready(const state& s, const leg_controller::data& d) -> bool {
	return s.double_support &&
		s.capture_forward <= d.weight_shift_capture_forward_limit &&
		s.capture_forward >= -d.weight_shift_capture_backward_limit &&
		gse::abs(s.capture_right) <= d.weight_shift_capture_right_limit;
}

auto gs::locomotion::write_targets(const skeleton_refs& r, const leg_joint_targets& targets, gse::write<controlled_joint_component>& ctrls, const bool controllers_active, const leg_controller::data& d) -> void {
	auto apply = [&](const gse::id joint_id, const gse::angle target) {
		if (auto* cj = ctrls.find(joint_id)) {
			cj->enabled = controllers_active;
			cj->target_angle = target;
			cj->gain = d.joint_gain;
			cj->damping = d.joint_damping;
		}
	};
	apply(r.hip_l_joint_id, targets.hip_l);
	apply(r.knee_l_joint_id, targets.knee_l);
	apply(r.hip_r_joint_id, targets.hip_r);
	apply(r.knee_r_joint_id, targets.knee_r);
}

auto gs::locomotion::foot_anchor_velocity(const gse::vec3<gse::position>& current, const gse::vec3<gse::position>& target, const leg_controller::data& d) -> gse::vec3<gse::velocity> {
	const auto delta = target - current;
	auto result = gse::vec3<gse::velocity>(
		gse::meters_per_second(1.f) * (d.foot_anchor_gain * delta.x()),
		gse::meters_per_second(0.f),
		gse::meters_per_second(1.f) * (d.foot_anchor_gain * delta.z())
	);

	const auto speed = gse::magnitude(result);
	if (speed > d.foot_anchor_max_speed && speed > gse::meters_per_second(0.001f)) {
		result *= d.foot_anchor_max_speed / speed;
	}
	return result;
}

auto gs::locomotion::write_foot_motor(const gse::id foot_id, const gse::vec3<gse::position>& current, const gse::vec3<gse::position>& target, const bool active, gse::write<gse::physics::motor_component>& motors, const leg_controller::data& d) -> void {
	if (auto* motor = motors.find(foot_id)) {
		motor->horizontal_only = true;
		motor->requires_ground_contact = true;
		if (!active) {
			motor->velocity_drive_target = {};
			motor->max_force = gse::newtons(0.f);
			return;
		}
		motor->velocity_drive_target = foot_anchor_velocity(current, target, d);
		motor->max_force = d.foot_anchor_max_force;
	}
}

auto gs::locomotion::leg_controller::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [refs, states, gaits, plans, contexts, ctrls, motors] = co_await ctx.acquire_with(
				gse::read_v<skeleton_refs>,
				gse::read_v<state>,
				gse::read_v<gait>,
				gse::read_v<plan>,
				gse::write_v<leg_context>,
				gse::write_v<controlled_joint_component>,
				gse::write_v<gse::physics::motor_component>
			);

			const bool log_now = d.log_timer.tick();
			const auto owner_ids = contexts.owner_ids();
			for (std::size_t i = 0; i < contexts.size(); ++i) {
				auto& cctx = contexts[i];
				const auto owner = owner_ids[i];

				const auto* r = refs.find(owner);
				const auto* s = states.find(owner);
				const auto* g = gaits.find(owner);
				const auto* p = plans.find(owner);
				if (!r || !s || !g || !p) {
					continue;
				}

				if (!cctx.planted_initialized) {
					cctx.planted_foot_l = grounded_foot_position(s->foot_position_l, d);
					cctx.planted_foot_r = grounded_foot_position(s->foot_position_r, d);
					cctx.planted_initialized = true;
				}

				const bool entered_swing = g->current == phase::swing &&
					(cctx.last_phase != phase::swing || cctx.last_swing_leg != g->swing_leg);
				const bool swing_foot_grounded = foot_is_grounded(g->swing_leg, *s);
				if (entered_swing) {
					cctx.swing_start_foot = current_foot_position(g->swing_leg, *s);
					cctx.swing_target_at_start = p->foot_target_world;
					cctx.swing_initialized = true;
				}
				if (g->current == phase::plant && swing_foot_grounded) {
					planted_foot_position(g->swing_leg, cctx) = grounded_foot_position(
						current_foot_position(g->swing_leg, *s),
						d
					);
				}
				if (g->current != phase::swing) {
					cctx.swing_initialized = false;
				}
				cctx.last_phase = g->current;
				cctx.last_swing_leg = g->swing_leg;

				const bool controllers_active = !g->fallen && s->valid;

				leg_joint_targets targets;
				if (controllers_active) {
					const bool hold_weight_shift = g->current == phase::weight_shift && !weight_shift_ready(*s, d);
					const bool plant_waiting_for_contact = g->current == phase::plant && !swing_foot_grounded;
					const bool both_stance = g->current == phase::idle || hold_weight_shift ||
						(g->current == phase::plant && !plant_waiting_for_contact);
					if (both_stance) {
						const auto [hip_l, knee_l] = compute_stance(leg::left, *s, *r, cctx, d);
						const auto [hip_r, knee_r] = compute_stance(leg::right, *s, *r, cctx, d);
						targets.hip_l = hip_l;
						targets.knee_l = knee_l;
						targets.hip_r = hip_r;
						targets.knee_r = knee_r;
					}
					else {
						const leg stance_leg = other(g->swing_leg);
						const auto [stance_hip, stance_knee] = compute_stance(stance_leg, *s, *r, cctx, d);
						const auto [swing_hip, swing_knee] = compute_swing(
							g->swing_leg,
							*s,
							*g,
							*p,
							*r,
							cctx,
							d
						);
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

				write_targets(*r, targets, ctrls, controllers_active, d);
				const bool left_anchor_active =
					controllers_active && s->foot_grounded_l && (g->current != phase::swing || g->swing_leg != leg::left);
				const bool right_anchor_active =
					controllers_active && s->foot_grounded_r && (g->current != phase::swing || g->swing_leg != leg::right);
				write_foot_motor(
					r->foot_l_id,
					s->foot_position_l,
					grounded_foot_position(cctx.planted_foot_l, d),
					left_anchor_active,
					motors,
					d
				);
				write_foot_motor(
					r->foot_r_id,
					s->foot_position_r,
					grounded_foot_position(cctx.planted_foot_r, d),
					right_anchor_active,
					motors,
					d
				);

				if (log_now) {
					gse::log::println(
						"leg_controller: owner={} phase={} swing={} fallen={} "
						"hips=({:+.3f},{:+.3f}) knees=({:+.3f},{:+.3f}) "
						"pelvis_y={:+.2f} target_y={:+.2f} lean_body=({:+.3f},{:+.3f}) "
						"feet_y=({:+.2f},{:+.2f}) grounded=({},{}) plants=({:+.2f},{:+.2f})/({:+.2f},{:+.2f})",
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
						-s->lean_body.z(),
						s->foot_position_l.y(),
						s->foot_position_r.y(),
						s->foot_grounded_l,
						s->foot_grounded_r,
						cctx.planted_foot_l.x(),
						cctx.planted_foot_l.z(),
						cctx.planted_foot_r.x(),
						cctx.planted_foot_r.z()
					);
				}
			}
		}

		co_await ctx.next_tick();
	}
}
