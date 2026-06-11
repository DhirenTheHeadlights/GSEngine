export module gs:locomotion_types;

import std;
import gse;

export namespace gs::locomotion {
	enum class leg : std::uint8_t {
		left,
		right
	};

	enum class phase : std::uint8_t {
		idle,
		weight_shift,
		swing,
		plant,
		recover
	};

	struct skeleton_refs {
		gse::id pelvis_id;
		gse::id torso_id;
		gse::id thigh_l_id;
		gse::id shin_l_id;
		gse::id foot_l_id;
		gse::id toe_l_id;
		gse::id thigh_r_id;
		gse::id shin_r_id;
		gse::id foot_r_id;
		gse::id toe_r_id;
		gse::mass pelvis_mass = gse::kilograms(3.f);
		gse::mass upper_body_mass = gse::kilograms(51.f);
		gse::mass thigh_mass = gse::kilograms(7.5f);
		gse::mass shin_mass = gse::kilograms(3.4f);
		gse::mass foot_mass = gse::kilograms(1.f);
		gse::id hip_l_joint_id;
		gse::id knee_l_joint_id;
		gse::id ankle_l_joint_id;
		gse::id hip_r_joint_id;
		gse::id knee_r_joint_id;
		gse::id ankle_r_joint_id;
		gse::id shoulder_l_joint_id;
		gse::id shoulder_r_joint_id;
		gse::length thigh_length = gse::meters(0.45f);
		gse::length shin_length = gse::meters(0.40f);
		gse::displacement hip_offset_lateral = gse::meters(0.075f);
		gse::displacement hip_offset_below_pelvis = gse::meters(0.10f);
		gse::position pelvis_target_height = gse::meters(0.90f);
	};

	struct intent {
		float forward = 0.f;
		float strafe = 0.f;
		float intensity = 0.f;
		float sprint_blend = 0.f;
		gse::angle desired_yaw;
		bool has_heading = false;
		bool sprint = false;
		bool jump = false;
	};

	struct state {
		bool valid = false;

		gse::vec3<gse::position> pelvis_position;
		gse::vec3<gse::velocity> pelvis_velocity;
		gse::quat pelvis_orientation = gse::quat(1.f, 0.f, 0.f, 0.f);
		gse::vec3f pelvis_forward = { 0.f, 0.f, -1.f };
		gse::vec3f pelvis_right = { 1.f, 0.f, 0.f };

		gse::vec3<gse::position> foot_position_l;
		gse::vec3<gse::position> foot_position_r;
		bool foot_grounded_l = false;
		bool foot_grounded_r = false;
		bool any_foot_grounded = false;
		bool double_support = false;

		gse::vec3<gse::position> support_min;
		gse::vec3<gse::position> support_max;
		gse::vec3<gse::position> support_center;

		gse::vec3<gse::position> com_world;
		gse::vec3<gse::displacement> lean_world;
		gse::vec3<gse::displacement> lean_body;
		gse::vec3<gse::velocity> velocity_world;
		gse::vec3<gse::velocity> velocity_body;
		gse::vec3<gse::displacement> capture_offset_world;
		gse::vec3<gse::displacement> capture_offset_body;
		gse::displacement capture_forward;
		gse::displacement capture_right;
		gse::velocity horizontal_speed;
		gse::time pendulum_time = gse::seconds(0.32f);

		gse::angle hip_angle_l;
		gse::angle knee_angle_l;
		gse::angle hip_angle_r;
		gse::angle knee_angle_r;
		gse::angle pelvis_pitch;
		gse::angular_velocity pelvis_pitch_rate;
	};

	struct gait {
		phase current = phase::idle;
		leg swing_leg = leg::left;
		gse::time phase_elapsed = gse::seconds(0.f);
		gse::time phase_duration = gse::seconds(0.f);
		int runaway_ticks = 0;
		bool fallen = false;
	};

	struct plan {
		gse::vec3<gse::position> foot_target_world;
		leg swing_leg = leg::left;
		gse::displacement capture_forward_at_commit;
		gse::displacement capture_right_at_commit;
		bool target_valid = false;
		bool locked = false;
		bool refined = false;
	};

	struct leg_context {
		phase last_phase = phase::idle;
		leg last_swing_leg = leg::left;
		gse::angle cop_trim;
		gse::angle cop_trim_applied;
		float arm_phase = 0.f;
		gse::vec3<gse::position> swing_start_foot;
		gse::vec3<gse::position> swing_target_at_start;
		gse::vec3<gse::position> planted_foot_l;
		gse::vec3<gse::position> planted_foot_r;
		bool swing_initialized = false;
		bool plant_initialized = false;
		bool planted_initialized = false;
	};

	auto other(
		leg l
	) -> leg;
	auto side_of(
		leg l
	) -> float;
	auto phase_progress(
		const gait& g
	) -> float;
	auto heading_error(
		const state& s,
		const intent& it
	) -> gse::angle;
}

auto gs::locomotion::other(const leg l) -> leg {
	return l == leg::left ? leg::right : leg::left;
}

auto gs::locomotion::side_of(const leg l) -> float {
	return l == leg::left ? -1.f : 1.f;
}

auto gs::locomotion::phase_progress(const gait& g) -> float {
	if (g.phase_duration <= gse::seconds(0.f)) {
		return 1.f;
	}
	return std::clamp(g.phase_elapsed / g.phase_duration, 0.f, 1.f);
}

auto gs::locomotion::heading_error(const state& s, const intent& it) -> gse::angle {
	if (!it.has_heading) {
		return gse::radians(0.f);
	}
	const float current_yaw = std::atan2(-s.pelvis_forward.x(), -s.pelvis_forward.z());
	return gse::radians(std::remainder(
		static_cast<float>(it.desired_yaw) - current_yaw,
		2.f * std::numbers::pi_v<float>
	));
}
