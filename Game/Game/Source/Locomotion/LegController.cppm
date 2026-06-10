export module gs:leg_controller;

import std;
import gse;

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
			gse::displacement swing_lift_height = gse::meters(0.20f);

			[[
				= gse::settings::describe<"Time used for swing-foot lift and descent.">{}
			]]
			gse::time swing_trajectory_duration = gse::seconds(0.36f);

			[[
				= gse::settings::describe<"Clamp range on stance hip target.">{}
			]]
			gse::angle stance_hip_clamp = gse::radians(0.90f);

			[[
				= gse::settings::describe<"Clamp range on stance knee target.">{}
			]]
			gse::angle stance_knee_clamp = gse::radians(1.20f);

			[[
				= gse::settings::describe<"Foot center Y used by stance IK.">{}
			]]
			gse::position stance_foot_ground_y = gse::meters(0.025f);

			[[
				= gse::settings::describe<"Maximum horizontal swing-foot travel while planting before contact.">{}
			]]
			gse::displacement plant_horizontal_reach = gse::meters(0.24f);

			[[
				= gse::settings::describe<"Distance below ground used as the active plant target before contact.">{}
			]]
			gse::displacement plant_foot_sink = gse::meters(0.05f);

			[[
				= gse::settings::describe<"Forward capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_forward_limit = gse::meters(0.45f);

			[[
				= gse::settings::describe<"Backward capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_backward_limit = gse::meters(0.30f);

			[[
				= gse::settings::describe<"Lateral capture limit for releasing the swing leg during weight shift.">{}
			]]
			gse::displacement weight_shift_capture_right_limit = gse::meters(0.32f);

			[[
				= gse::settings::describe<"Stance hip pitch servo stiffness.">{}
			]]
			gse::angular_stiffness hip_drive_stiffness = gse::newton_meters_per_radian(350.f);

			[[
				= gse::settings::describe<"Swing hip pitch servo stiffness (kept low to limit trunk reaction).">{}
			]]
			gse::angular_stiffness swing_hip_stiffness = gse::newton_meters_per_radian(250.f);

			[[
				= gse::settings::describe<"Hip roll/yaw servo stiffness holding the leg in the sagittal plane.">{}
			]]
			gse::angular_stiffness hip_lateral_stiffness = gse::newton_meters_per_radian(250.f);

			[[
				= gse::settings::describe<"Knee servo stiffness.">{}
			]]
			gse::angular_stiffness knee_drive_stiffness = gse::newton_meters_per_radian(300.f);

			[[
				= gse::settings::describe<"Ankle servo stiffness.">{}
			]]
			gse::angular_stiffness ankle_drive_stiffness = gse::newton_meters_per_radian(300.f);

			[[
				= gse::settings::describe<"Joint servo damping as a multiple of stiffness times substep dt.">{}
			]]
			float drive_damping = 6.f;

			[[
				= gse::settings::describe<"Stance hip servo torque limit.">{}
			]]
			gse::torque hip_max_torque = gse::newton_meters(250.f);

			[[
				= gse::settings::describe<"Stance knee servo torque limit.">{}
			]]
			gse::torque knee_max_torque = gse::newton_meters(250.f);

			[[
				= gse::settings::describe<"Stance ankle servo torque limit.">{}
			]]
			gse::torque ankle_max_torque = gse::newton_meters(140.f);

			[[
				= gse::settings::describe<"Swing hip servo torque limit (limits trunk reaction).">{}
			]]
			gse::torque swing_hip_max_torque = gse::newton_meters(90.f);

			[[
				= gse::settings::describe<"Swing knee servo torque limit.">{}
			]]
			gse::torque swing_knee_max_torque = gse::newton_meters(80.f);

			[[
				= gse::settings::describe<"Swing ankle servo torque limit.">{}
			]]
			gse::torque swing_ankle_max_torque = gse::newton_meters(30.f);

			[[
				= gse::settings::describe<"Trailing-ankle plantarflexion added across weight shift (toe-off push).">{}
			]]
			gse::angle toe_off_angle = gse::radians(-0.20f);

			[[
				= gse::settings::describe<"Swing-hip roll target scale toward the planned lateral foot offset (0 disables).">{}
			]]
			float swing_roll_gain = 0.f;

			[[
				= gse::settings::describe<"Stance-hip yaw target per radian of heading error (turns the pelvis over the planted foot).">{}
			]]
			float steer_gain = 1.5f;

			[[
				= gse::settings::describe<"Clamp on the stance-hip steering yaw target.">{}
			]]
			gse::angle steer_clamp = gse::radians(0.55f);

			[[
				= gse::settings::describe<"Stance-ankle CoP shift per radian of pelvis pitch, righting the trunk.">{}
			]]
			float pelvis_righting_gain = 1.0f;

			[[
				= gse::settings::describe<"Pitch-rate anticipation horizon for the stance-ankle CoP shift.">{}
			]]
			gse::time pelvis_righting_rate_gain = gse::seconds(0.20f);

			[[
				= gse::settings::describe<"Clamp on the stance-ankle CoP shift.">{}
			]]
			gse::angle pelvis_righting_clamp = gse::radians(0.40f);

			[[
				= gse::settings::describe<"Adaptation rate of the standing ankle CoP trim per second of pitch error.">{}
			]]
			float posture_trim_rate = 0.25f;

			[[
				= gse::settings::describe<"Clamp on the adaptive ankle CoP trim.">{}
			]]
			gse::angle posture_trim_clamp = gse::radians(0.30f);

			[[
				= gse::settings::describe<"Fraction of the posture trim also applied as a stance-hip bias, unloading the ankle.">{}
			]]
			float posture_trim_hip_share = 0.5f;

			[[
				= gse::settings::describe<"Grounded foot anchor velocity gain.">{}
			]]
			gse::inverse_length foot_anchor_gain = gse::per_meter(4.f);

			[[
				= gse::settings::describe<"Maximum grounded foot anchor speed.">{}
			]]
			gse::velocity foot_anchor_max_speed = gse::meters_per_second(2.5f);

			[[
				= gse::settings::describe<"Maximum grounded foot anchor force.">{}
			]]
			gse::force foot_anchor_max_force = gse::newtons(700.f);

			[[
				= gse::settings::describe<"Maximum foot anchor force for the airborne swing foot.">{}
			]]
			gse::force swing_foot_anchor_max_force = gse::newtons(150.f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			data& d,
			gse::read<skeleton_refs> refs,
			gse::read<intent> intents,
			gse::read<state> states,
			gse::read<gait> gaits,
			gse::read<plan> plans,
			gse::write<leg_context> contexts,
			gse::write<gse::physics::joint_drive_component> drives,
			gse::write<gse::physics::motor_component> motors
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	struct leg_pose {
		gse::angle hip = gse::radians(0.f);
		gse::angle knee = gse::radians(0.f);
		gse::angle ankle = gse::radians(0.f);
		gse::angle hip_yaw = gse::radians(0.f);
		gse::angle hip_roll = gse::radians(0.f);
	};

	struct leg_joint_targets {
		leg_pose left;
		leg_pose right;
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
	) -> leg_pose;
	auto stance_foot_target(
		leg which,
		const leg_context& ctx,
		const leg_controller::data& d
	) -> gse::vec3<gse::position>;
	auto current_foot_position(
		leg which,
		const state& s
	) -> const gse::vec3<gse::position>&;
	auto leg_target_reached(
		leg which,
		const state& s,
		const plan& p,
		const leg_controller::data& d
	) -> bool;
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
	) -> leg_pose;
	auto weight_shift_ready(
		const state& s,
		const leg_controller::data& d
	) -> bool;
	auto pelvis_yaw_orientation(
		const state& s
	) -> gse::quat;
	auto pelvis_pitch_about_right(
		const state& s
	) -> gse::angle;
	auto steering_target(
		const state& s,
		const intent& it,
		const leg_controller::data& d
	) -> gse::angle;
	auto write_drives(
		const skeleton_refs& r,
		const leg_joint_targets& targets,
		leg swing_leg,
		bool swing_active,
		bool swing_airborne,
		gse::write<gse::physics::joint_drive_component>& drives,
		bool controllers_active,
		const leg_controller::data& d
	) -> void;
	auto foot_anchor_velocity(
		const gse::vec3<gse::position>& current,
		const gse::vec3<gse::position>& target,
		bool include_vertical,
		bool allow_upward_vertical,
		const leg_controller::data& d
	) -> gse::vec3<gse::velocity>;
	auto write_foot_motor(
		gse::id foot_id,
		const gse::vec3<gse::position>& current,
		const gse::vec3<gse::position>& target,
		bool active,
		bool requires_ground_contact,
		bool include_vertical,
		bool allow_upward_vertical,
		gse::force max_force,
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

auto gs::locomotion::leg_target_reached(const leg which, const state& s, const plan& p, const leg_controller::data& d) -> bool {
	if (!p.target_valid || p.swing_leg != which) {
		return true;
	}

	const auto delta = current_foot_position(which, s) - p.foot_target_world;
	const auto horizontal_error = gse::hypot(delta.x(), delta.z());
	return horizontal_error <= d.plant_horizontal_reach * 0.5f;
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
	return gse::vec3<gse::position>(grounded_current.x() + delta.x() * scale,
									plant_y,
									grounded_current.z() + delta.z() * scale);
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
		const auto clamped_rel_body = gse::vec3<gse::displacement>(rel_body.x(), -clamped_drop, gse::meters(0.f));
		return hip_world + gse::rotate_vector(pelvis_orientation, clamped_rel_body);
	}

	const auto max_sagittal = gse::sqrt(max_reach * max_reach - drop * drop);
	if (gse::abs(sagittal) <= max_sagittal) {
		return target;
	}

	const auto clamped_sagittal = std::clamp<gse::length>(sagittal, -max_sagittal, max_sagittal);
	const auto clamped_rel_body = gse::vec3<gse::displacement>(rel_body.x(), rel_body.y(), -clamped_sagittal);
	return hip_world + gse::rotate_vector(pelvis_orientation, clamped_rel_body);
}

auto gs::locomotion::stance_foot_target(const leg which, const leg_context& ctx, const leg_controller::data& d) -> gse::vec3<gse::position> {
	return grounded_foot_position(planted_foot_position(which, ctx), d);
}

auto gs::locomotion::compute_stance(const leg which, const state& s, const skeleton_refs& r, const leg_context& ctx, const leg_controller::data& d) -> leg_pose {
	auto support_state = s;
	support_state.pelvis_position.y() = std::max(support_state.pelvis_position.y(), r.pelvis_target_height);
	support_state.pelvis_orientation = pelvis_yaw_orientation(s);

	const auto hip_world = hip_world_position(support_state, r, which);
	const auto foot_target = stance_foot_target(which, ctx, d);
	const auto ik = solve_leg_ik(hip_world, foot_target, support_state.pelvis_orientation, r.thigh_length, r.shin_length);

	const auto pitch = pelvis_pitch_about_right(s);
	const auto hip_target = std::clamp(
		ik.hip_pitch - ctx.cop_trim_applied * d.posture_trim_hip_share,
		-d.stance_hip_clamp,
		d.stance_hip_clamp
	);
	const auto knee_target = std::clamp(ik.knee_bend, -d.stance_knee_clamp, gse::radians(0.f));
	const auto cop_shift = std::clamp(
		pitch * d.pelvis_righting_gain + s.pelvis_pitch_rate * d.pelvis_righting_rate_gain,
		-d.pelvis_righting_clamp,
		d.pelvis_righting_clamp
	);
	const auto ankle_target = -(hip_target + knee_target) + cop_shift + ctx.cop_trim_applied;

	return {
		.hip = hip_target,
		.knee = knee_target,
		.ankle = ankle_target,
	};
}

auto gs::locomotion::compute_swing(const leg which, const state& s, const gait& g, const plan& p, const skeleton_refs& r, const leg_context& ctx, const leg_controller::data& d) -> leg_pose {
	const auto pitch = pelvis_pitch_about_right(s);
	const auto flat_ankle = [&](const gse::angle hip, const gse::angle knee) {
		return -(pitch + hip + knee);
	};

	if (g.current == phase::idle || g.current == phase::recover) {
		return {
			.hip = gse::radians(0.f),
			.knee = d.stance_knee_rest,
			.ankle = flat_ankle(gse::radians(0.f), d.stance_knee_rest),
		};
	}

	if (g.current == phase::weight_shift) {
		const float t = phase_progress(g);
		const auto knee = d.stance_knee_rest + (d.swing_knee_lift * 0.15f - d.stance_knee_rest) * t;
		const float toe_off_fade = std::clamp((gse::meters(0.45f) - s.capture_forward) / gse::meters(0.30f), 0.f, 1.f);
		return {
			.hip = gse::radians(0.f),
			.knee = knee,
			.ankle = flat_ankle(gse::radians(0.f), knee) + d.toe_off_angle * (t * toe_off_fade),
		};
	}

	const auto start_foot =
		ctx.swing_initialized ? ctx.swing_start_foot : (which == leg::left ? s.foot_position_l : s.foot_position_r);
	const bool target_matches = p.target_valid && p.swing_leg == which;
	auto target_foot = target_matches ? p.foot_target_world : start_foot;
	if (g.current == phase::plant) {
		target_foot = foot_is_grounded(which, s) &&
				target_matches
			? grounded_foot_position(p.foot_target_world, d)
			: (target_matches ? plant_contact_target(current_foot_position(which, s), p.foot_target_world, d)
							  : grounded_foot_position(current_foot_position(which, s), d));
	}
	const float t = swing_trajectory_progress(g, d);
	const float trajectory_t = g.current == phase::plant ? 1.f : t;
	const auto lift = g.current == phase::plant ? gse::displacement{} : d.swing_lift_height;

	auto ik_state = s;
	ik_state.pelvis_orientation = pelvis_yaw_orientation(s);
	const auto hip_world = hip_world_position(ik_state, r, which);
	const auto desired_foot = clamp_foot_to_leg_reach(
		hip_world,
		compute_swing_foot(start_foot, target_foot, trajectory_t, lift),
		ik_state.pelvis_orientation,
		r.thigh_length,
		r.shin_length
	);
	const auto ik = solve_leg_ik(hip_world, desired_foot, ik_state.pelvis_orientation, r.thigh_length, r.shin_length);

	const auto rel_body = gse::inverse_rotate_vector(ik_state.pelvis_orientation, desired_foot - hip_world);
	const auto hip_roll = gse::radians(std::atan2(
		static_cast<float>(rel_body.x()),
		std::max(static_cast<float>(-rel_body.y()), 0.05f)
	)) * d.swing_roll_gain;

	const auto hip_target = ik.hip_pitch - pitch;
	return {
		.hip = hip_target,
		.knee = ik.knee_bend,
		.ankle = flat_ankle(hip_target, ik.knee_bend),
		.hip_roll = hip_roll,
	};
}

auto gs::locomotion::weight_shift_ready(const state& s, const leg_controller::data& d) -> bool {
	return s.double_support &&
		s.capture_forward <= d.weight_shift_capture_forward_limit &&
		s.capture_forward >= -d.weight_shift_capture_backward_limit &&
		gse::abs(s.capture_right) <= d.weight_shift_capture_right_limit;
}

auto gs::locomotion::pelvis_yaw_orientation(const state& s) -> gse::quat {
	const auto fwd_xz = gse::vec3f(s.pelvis_forward.x(), 0.f, s.pelvis_forward.z());
	const float len = gse::magnitude(fwd_xz);
	if (len <= 0.0001f) {
		return gse::quat(1.f, 0.f, 0.f, 0.f);
	}
	const auto yaw = gse::radians(std::atan2(-fwd_xz.x() / len, -fwd_xz.z() / len));
	return gse::quat(gse::vec3f(0.f, 1.f, 0.f), yaw);
}

auto gs::locomotion::pelvis_pitch_about_right(const state& s) -> gse::angle {
	return gse::radians(std::asin(std::clamp(s.pelvis_forward.y(), -1.f, 1.f)));
}

auto gs::locomotion::steering_target(const state& s, const intent& it, const leg_controller::data& d) -> gse::angle {
	if (!it.has_heading) {
		return gse::radians(0.f);
	}
	const float current_yaw = std::atan2(-s.pelvis_forward.x(), -s.pelvis_forward.z());
	const float yaw_error = std::remainder(static_cast<float>(it.desired_yaw) - current_yaw, 2.f * std::numbers::pi_v<float>);
	return std::clamp(gse::radians(-yaw_error * d.steer_gain), -d.steer_clamp, d.steer_clamp);
}

auto gs::locomotion::write_drives(const skeleton_refs& r, const leg_joint_targets& targets, const leg swing_leg, const bool swing_active, const bool swing_airborne, gse::write<gse::physics::joint_drive_component>& drives, const bool controllers_active, const leg_controller::data& d) -> void {
	auto apply = [&](const gse::id joint_id, const gse::vec3<gse::angle>& target, const gse::angular_stiffness primary, const gse::angular_stiffness lateral, const gse::torque max_torque) {
		if (auto* drv = drives.find(joint_id)) {
			drv->enabled = controllers_active;
			drv->target = target;
			drv->stiffness = gse::vec3<gse::angular_stiffness>(primary, lateral, lateral);
			drv->damping = d.drive_damping;
			drv->max_torque = max_torque;
		}
	};
	const auto hinge_lateral = gse::newton_meters_per_radian(0.f);
	auto apply_leg = [&](const leg which, const leg_pose& pose, const gse::id hip_id, const gse::id knee_id, const gse::id ankle_id) {
		const bool swinging = swing_active && which == swing_leg;
		const bool airborne = swing_airborne && which == swing_leg;
		const auto hip_stiffness = swinging ? d.swing_hip_stiffness : d.hip_drive_stiffness;
		const auto hip_torque = swinging ? d.swing_hip_max_torque : d.hip_max_torque;
		const auto knee_torque = swinging ? d.swing_knee_max_torque : d.knee_max_torque;
		const auto ankle_torque = airborne ? d.swing_ankle_max_torque : d.ankle_max_torque;
		apply(hip_id, gse::vec3<gse::angle>(pose.hip, pose.hip_yaw, pose.hip_roll), hip_stiffness, d.hip_lateral_stiffness, hip_torque);
		apply(knee_id, gse::vec3<gse::angle>(pose.knee, gse::radians(0.f), gse::radians(0.f)), d.knee_drive_stiffness, hinge_lateral, knee_torque);
		apply(ankle_id, gse::vec3<gse::angle>(pose.ankle, gse::radians(0.f), gse::radians(0.f)), d.ankle_drive_stiffness, hinge_lateral, ankle_torque);
	};
	apply_leg(leg::left, targets.left, r.hip_l_joint_id, r.knee_l_joint_id, r.ankle_l_joint_id);
	apply_leg(leg::right, targets.right, r.hip_r_joint_id, r.knee_r_joint_id, r.ankle_r_joint_id);
}

auto gs::locomotion::foot_anchor_velocity(const gse::vec3<gse::position>& current, const gse::vec3<gse::position>& target, const bool include_vertical, const bool allow_upward_vertical, const leg_controller::data& d) -> gse::vec3<gse::velocity> {
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

	if (include_vertical && (allow_upward_vertical || delta.y() < gse::meters(0.f))) {
		auto vertical = gse::meters_per_second(1.f) * (d.foot_anchor_gain * delta.y());
		if (allow_upward_vertical) {
			vertical = std::clamp(vertical, -d.foot_anchor_max_speed, d.foot_anchor_max_speed);
		}
		else if (vertical < -d.foot_anchor_max_speed) {
			vertical = -d.foot_anchor_max_speed;
		}
		result.y() = vertical;
	}

	return result;
}

auto gs::locomotion::write_foot_motor(const gse::id foot_id, const gse::vec3<gse::position>& current, const gse::vec3<gse::position>& target, const bool active, const bool requires_ground_contact, const bool include_vertical, const bool allow_upward_vertical, const gse::force max_force, gse::write<gse::physics::motor_component>& motors, const leg_controller::data& d) -> void {
	if (auto* motor = motors.find(foot_id)) {
		motor->horizontal_only = !include_vertical;
		motor->requires_ground_contact = requires_ground_contact;
		if (!active) {
			motor->velocity_drive_target = {};
			motor->max_force = gse::newtons(0.f);
			return;
		}
		motor->velocity_drive_target = foot_anchor_velocity(current, target, include_vertical, allow_upward_vertical, d);
		motor->max_force = max_force;
	}
}

auto gs::locomotion::leg_controller::run(data& d, gse::read<skeleton_refs> refs, gse::read<intent> intents, gse::read<state> states, gse::read<gait> gaits, gse::read<plan> plans, gse::write<leg_context> contexts, gse::write<gse::physics::joint_drive_component> drives, gse::write<gse::physics::motor_component> motors) -> gse::async::task<> {
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
		if (!r || !s || !g || !p) {
			continue;
		}

		if (s->valid && !cctx.planted_initialized) {
			cctx.planted_foot_l = grounded_foot_position(s->foot_position_l, d);
			cctx.planted_foot_r = grounded_foot_position(s->foot_position_r, d);
			cctx.planted_initialized = true;
		}

		const bool entered_swing = g->current == phase::swing &&
			(cctx.last_phase != phase::swing || cctx.last_swing_leg != g->swing_leg);
		const bool entered_plant = g->current == phase::plant &&
			(cctx.last_phase != phase::plant || cctx.last_swing_leg != g->swing_leg);
		const bool entered_recover = g->current == phase::recover && cctx.last_phase != phase::recover;
		const bool swing_foot_grounded = foot_is_grounded(g->swing_leg, *s);
		if (entered_swing) {
			cctx.swing_start_foot = current_foot_position(g->swing_leg, *s);
			cctx.swing_target_at_start = p->foot_target_world;
			cctx.swing_initialized = true;
		}
		if (entered_plant) {
			cctx.plant_initialized = false;
		}
		if (entered_recover) {
			cctx.planted_foot_l = grounded_foot_position(hip_world_position(*s, *r, leg::left), d);
			cctx.planted_foot_r = grounded_foot_position(hip_world_position(*s, *r, leg::right), d);
		}
		const bool plant_target_matches = p->target_valid && p->swing_leg == g->swing_leg;
		const bool plant_target_reached = leg_target_reached(g->swing_leg, *s, *p, d);
		if (g->current == phase::plant && swing_foot_grounded && !cctx.plant_initialized) {
			planted_foot_position(g->swing_leg, cctx) = plant_target_matches && plant_target_reached ? grounded_foot_position(p->foot_target_world, d)
																									 : grounded_foot_position(current_foot_position(g->swing_leg, *s), d);
			cctx.plant_initialized = true;
		}
		if (g->current != phase::swing) {
			cctx.swing_initialized = false;
		}
		if (g->current != phase::plant) {
			cctx.plant_initialized = false;
		}
		cctx.last_phase = g->current;
		cctx.last_swing_leg = g->swing_leg;

		const bool controllers_active = !g->fallen && s->valid && cctx.planted_initialized;

		const float walk_intensity = it ? std::clamp(it->intensity, 0.f, 1.f) : 0.f;
		if (controllers_active && s->any_foot_grounded && walk_intensity > 0.3f) {
			const auto frame_dt = gse::system_clock::fixed_dt<gse::time>() * gse::system_clock::fixed_steps_this_frame();
			cctx.cop_trim = std::clamp(
				cctx.cop_trim + s->pelvis_pitch * (d.posture_trim_rate * frame_dt.as<gse::seconds>()),
				-d.posture_trim_clamp,
				d.posture_trim_clamp
			);
		}
		cctx.cop_trim_applied = cctx.cop_trim * std::clamp(walk_intensity / 0.5f, 0.f, 1.f);

		leg_joint_targets targets;
		bool swing_active = false;
		if (controllers_active) {
			const bool hold_weight_shift = g->current == phase::weight_shift && !weight_shift_ready(*s, d);
			const bool plant_waiting_for_contact = g->current == phase::plant && !swing_foot_grounded;
			const bool both_stance = g->current == phase::idle || g->current == phase::recover || hold_weight_shift ||
				(g->current == phase::plant && !plant_waiting_for_contact);
			swing_active = !both_stance;
			const auto steer = it && s->valid ? steering_target(*s, *it, d) : gse::radians(0.f);
			if (both_stance) {
				targets.left = compute_stance(leg::left, *s, *r, cctx, d);
				targets.right = compute_stance(leg::right, *s, *r, cctx, d);
				targets.left.hip_yaw = steer;
				targets.right.hip_yaw = steer;
			}
			else {
				const leg stance_leg = other(g->swing_leg);
				auto stance_pose = compute_stance(stance_leg, *s, *r, cctx, d);
				const auto swing_pose = compute_swing(g->swing_leg, *s, *g, *p, *r, cctx, d);
				stance_pose.hip_yaw = steer;
				if (stance_leg == leg::left) {
					targets.left = stance_pose;
					targets.right = swing_pose;
				}
				else {
					targets.right = stance_pose;
					targets.left = swing_pose;
				}
			}
		}

		write_drives(*r, targets, g->swing_leg, swing_active, swing_active && g->current == phase::swing, drives, controllers_active, d);
		const bool left_planting =
			g->current == phase::plant && g->swing_leg == leg::left && plant_target_matches && !cctx.plant_initialized;
		const bool right_planting =
			g->current == phase::plant && g->swing_leg == leg::right && plant_target_matches && !cctx.plant_initialized;
		const bool left_swinging =
			g->current == phase::swing && g->swing_leg == leg::left && p->target_valid && p->swing_leg == leg::left;
		const bool right_swinging =
			g->current == phase::swing && g->swing_leg == leg::right && p->target_valid && p->swing_leg == leg::right;
		const bool left_stance = g->current != phase::swing || g->swing_leg != leg::left;
		const bool right_stance = g->current != phase::swing || g->swing_leg != leg::right;
		const bool left_airborne_stance = left_stance && !s->foot_grounded_l && !left_planting;
		const bool right_airborne_stance = right_stance && !s->foot_grounded_r && !right_planting;
		const bool left_supporting = left_stance && g->current != phase::idle;
		const bool right_supporting = right_stance && g->current != phase::idle;
		const bool left_anchor_active =
			controllers_active && ((s->foot_grounded_l && left_stance) || left_planting || left_airborne_stance || left_swinging);
		const bool right_anchor_active =
			controllers_active && ((s->foot_grounded_r && right_stance) || right_planting || right_airborne_stance || right_swinging);
		const auto left_anchor_target = left_swinging
			? compute_swing_foot(cctx.swing_start_foot, p->foot_target_world, swing_trajectory_progress(*g, d), d.swing_lift_height)
			: left_planting ? grounded_foot_position(p->foot_target_world, d)
			: grounded_foot_position(cctx.planted_foot_l, d);
		const auto right_anchor_target = right_swinging
			? compute_swing_foot(cctx.swing_start_foot, p->foot_target_world, swing_trajectory_progress(*g, d), d.swing_lift_height)
			: right_planting ? grounded_foot_position(p->foot_target_world, d)
			: grounded_foot_position(cctx.planted_foot_r, d);
		write_foot_motor(
			r->foot_l_id,
			s->foot_position_l,
			left_anchor_target,
			left_anchor_active,
			!(left_planting || left_airborne_stance || left_swinging),
			left_planting || left_airborne_stance || left_supporting || left_swinging,
			left_swinging,
			left_swinging ? d.swing_foot_anchor_max_force : d.foot_anchor_max_force,
			motors,
			d
		);
		write_foot_motor(
			r->foot_r_id,
			s->foot_position_r,
			right_anchor_target,
			right_anchor_active,
			!(right_planting || right_airborne_stance || right_swinging),
			right_planting || right_airborne_stance || right_supporting || right_swinging,
			right_swinging,
			right_swinging ? d.swing_foot_anchor_max_force : d.foot_anchor_max_force,
			motors,
			d
		);

		if (log_now) {
			gse::log::println(
				"leg_controller: owner={} phase={} swing={} fallen={} "
				"hips=({:+.3f},{:+.3f}) knees=({:+.3f},{:+.3f}) ankles=({:+.3f},{:+.3f}) "
				"pelvis_y={:+.2f} target_y={:+.2f} "
				"feet_y=({:+.2f},{:+.2f}) grounded=({},{}) plants=({:+.2f},{:+.2f})/({:+.2f},{:+.2f})",
				owner.number(),
				g->current,
				g->swing_leg,
				g->fallen,
				targets.left.hip,
				targets.right.hip,
				targets.left.knee,
				targets.right.knee,
				targets.left.ankle,
				targets.right.ankle,
				s->pelvis_position.y(),
				r->pelvis_target_height,
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

	return {};
}
