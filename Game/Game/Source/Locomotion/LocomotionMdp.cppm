export module gs:locomotion_mdp;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct observation {
		gse::displacement pelvis_height;
		gse::angle pelvis_pitch;
		gse::angular_velocity pelvis_pitch_rate;
		gse::vec3<gse::velocity> velocity_body;
		gse::angle hip_angle_l;
		gse::angle knee_angle_l;
		gse::angle ankle_angle_l;
		gse::angle hip_angle_r;
		gse::angle knee_angle_r;
		gse::angle ankle_angle_r;
		gse::angular_velocity hip_rate_l;
		gse::angular_velocity knee_rate_l;
		gse::angular_velocity ankle_rate_l;
		gse::angular_velocity hip_rate_r;
		gse::angular_velocity knee_rate_r;
		gse::angular_velocity ankle_rate_r;
		float foot_grounded_l = 0.f;
		float foot_grounded_r = 0.f;
		gse::displacement capture_forward;
		gse::displacement capture_right;
		gse::displacement lean_body_x;
		gse::displacement lean_body_z;
		float cmd_forward = 0.f;
		float cmd_strafe = 0.f;
		float cmd_sprint = 0.f;
		gse::angle heading_error;
		float phase_sin = 0.f;
		float phase_cos = 1.f;
	};

	struct action {
		gse::angle hip_l_pitch;
		gse::angle hip_l_yaw;
		gse::angle hip_l_roll;
		gse::angle knee_l;
		gse::angle ankle_l;
		gse::angle hip_r_pitch;
		gse::angle hip_r_yaw;
		gse::angle hip_r_roll;
		gse::angle knee_r;
		gse::angle ankle_r;
	};

	template <typename T>
	consteval auto scalar_count() -> std::size_t;

	auto observe(
		const state& s,
		const intent& it,
		const gait& g,
		float phi
	) -> observation;

	auto action_from_drives(
		const skeleton_refs& r,
		gse::read<gse::physics::joint_drive_component>& drives
	) -> action;

	auto apply_action(
		const action& act,
		const skeleton_refs& r,
		gse::write<gse::physics::joint_drive_component>& drives
	) -> void;

	auto pack_observation(
		const observation& obs,
		std::span<float> buf
	) -> void;

	auto unpack_action(
		std::span<const float> buf
	) -> action;

	auto locomotion_selftest() -> bool;
}

template <typename T>
consteval auto gs::locomotion::scalar_count() -> std::size_t {
	return sizeof(T) / sizeof(float);
}

auto gs::locomotion::observe(const state& s, const intent& it, const gait&, const float phi) -> observation {
	return {
		.pelvis_height = s.pelvis_position.y() - s.support_center.y(),
		.pelvis_pitch = s.pelvis_pitch,
		.pelvis_pitch_rate = s.pelvis_pitch_rate,
		.velocity_body = s.velocity_body,
		.hip_angle_l = s.hip_angle_l,
		.knee_angle_l = s.knee_angle_l,
		.ankle_angle_l = s.ankle_angle_l,
		.hip_angle_r = s.hip_angle_r,
		.knee_angle_r = s.knee_angle_r,
		.ankle_angle_r = s.ankle_angle_r,
		.hip_rate_l = s.hip_rate_l,
		.knee_rate_l = s.knee_rate_l,
		.ankle_rate_l = s.ankle_rate_l,
		.hip_rate_r = s.hip_rate_r,
		.knee_rate_r = s.knee_rate_r,
		.ankle_rate_r = s.ankle_rate_r,
		.foot_grounded_l = s.foot_grounded_l ? 1.f : 0.f,
		.foot_grounded_r = s.foot_grounded_r ? 1.f : 0.f,
		.capture_forward = s.capture_forward,
		.capture_right = s.capture_right,
		.lean_body_x = s.lean_body.x(),
		.lean_body_z = s.lean_body.z(),
		.cmd_forward = it.forward,
		.cmd_strafe = it.strafe,
		.cmd_sprint = it.sprint_blend,
		.heading_error = heading_error(s, it),
		.phase_sin = std::sin(phi * 2.f * std::numbers::pi_v<float>),
		.phase_cos = std::cos(phi * 2.f * std::numbers::pi_v<float>),
	};
}

auto gs::locomotion::action_from_drives(const skeleton_refs& r, gse::read<gse::physics::joint_drive_component>& drives) -> action {
	auto act = action{};
	if (const auto* drv = drives.find(r.hip_l_joint_id)) {
		act.hip_l_pitch = drv->target.x();
		act.hip_l_yaw = drv->target.y();
		act.hip_l_roll = drv->target.z();
	}
	if (const auto* drv = drives.find(r.knee_l_joint_id)) {
		act.knee_l = drv->target.x();
	}
	if (const auto* drv = drives.find(r.ankle_l_joint_id)) {
		act.ankle_l = drv->target.x();
	}
	if (const auto* drv = drives.find(r.hip_r_joint_id)) {
		act.hip_r_pitch = drv->target.x();
		act.hip_r_yaw = drv->target.y();
		act.hip_r_roll = drv->target.z();
	}
	if (const auto* drv = drives.find(r.knee_r_joint_id)) {
		act.knee_r = drv->target.x();
	}
	if (const auto* drv = drives.find(r.ankle_r_joint_id)) {
		act.ankle_r = drv->target.x();
	}
	return act;
}

auto gs::locomotion::pack_observation(const observation& obs, const std::span<float> buf) -> void {
	static_assert(sizeof(observation) == 30 * sizeof(float));
	std::memcpy(buf.data(), &obs, sizeof(observation));
}

auto gs::locomotion::unpack_action(const std::span<const float> buf) -> action {
	static_assert(sizeof(action) == 10 * sizeof(float));
	auto act = action{};
	std::memcpy(&act, buf.data(), sizeof(action));
	return act;
}

auto gs::locomotion::apply_action(const action& act, const skeleton_refs& r, gse::write<gse::physics::joint_drive_component>& drives) -> void {
	const auto write_target = [&](const gse::id joint_id, const gse::vec3<gse::angle>& target) {
		if (auto* drv = drives.find(joint_id)) {
			drv->target = target;
		}
	};
	write_target(r.hip_l_joint_id, gse::vec3<gse::angle>(act.hip_l_pitch, act.hip_l_yaw, act.hip_l_roll));
	write_target(r.knee_l_joint_id, gse::vec3<gse::angle>(act.knee_l, gse::radians(0.f), gse::radians(0.f)));
	write_target(r.ankle_l_joint_id, gse::vec3<gse::angle>(act.ankle_l, gse::radians(0.f), gse::radians(0.f)));
	write_target(r.hip_r_joint_id, gse::vec3<gse::angle>(act.hip_r_pitch, act.hip_r_yaw, act.hip_r_roll));
	write_target(r.knee_r_joint_id, gse::vec3<gse::angle>(act.knee_r, gse::radians(0.f), gse::radians(0.f)));
	write_target(r.ankle_r_joint_id, gse::vec3<gse::angle>(act.ankle_r, gse::radians(0.f), gse::radians(0.f)));
}

auto gs::locomotion::locomotion_selftest() -> bool {
	auto ok = true;

	const auto obs_n = scalar_count<observation>();
	const auto act_n = scalar_count<action>();
	if (obs_n != 30) {
		gse::log::println("locomotion_selftest: FAIL scalar_count<observation>={} expected 30", obs_n);
		ok = false;
	}
	if (act_n != 10) {
		gse::log::println("locomotion_selftest: FAIL scalar_count<action>={} expected 10", act_n);
		ok = false;
	}

	auto obs = observation{};
	obs.pelvis_pitch = gse::radians(0.3f);
	obs.velocity_body = gse::vec3<gse::velocity>(gse::meters_per_second(1.f), gse::meters_per_second(2.f), gse::meters_per_second(3.f));
	obs.foot_grounded_l = 1.0f;
	obs.phase_cos = 0.5f;

	auto buf = std::vector<float>(obs_n, 0.f);
	pack_observation(obs, buf);
	if (std::abs(buf[1] - 0.3f) > 1e-5f) {
		gse::log::println("locomotion_selftest: FAIL obs pitch slot={} expected 0.3", buf[1]);
		ok = false;
	}
	if (std::abs(buf[3] - 1.f) > 1e-5f || std::abs(buf[5] - 3.f) > 1e-5f) {
		gse::log::println("locomotion_selftest: FAIL obs velocity_body slots");
		ok = false;
	}

	const auto act_floats = std::array<float, 10>{ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
	const auto act = unpack_action(act_floats);
	auto act_round = std::array<float, 10>{};
	std::memcpy(act_round.data(), &act, sizeof(act));
	if (std::abs(act_round[0] - 0.1f) > 1e-5f || std::abs(act_round[9] - 1.0f) > 1e-5f) {
		gse::log::println("locomotion_selftest: FAIL action round-trip");
		ok = false;
	}

	if (gse::layout_hash<observation>() == 0 || gse::layout_hash<action>() == 0) {
		gse::log::println("locomotion_selftest: FAIL layout_hash zero");
		ok = false;
	}

	gse::log::println("locomotion_selftest: {} obs_dim={} act_dim={} obs_hash={:016x} act_hash={:016x}", ok ? "PASS" : "FAIL", obs_n, act_n, gse::layout_hash<observation>(), gse::layout_hash<action>());
	return ok;
}
