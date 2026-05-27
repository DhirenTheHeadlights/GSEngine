export module gs:leg_ik;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct leg_ik_result {
		gse::angle hip_pitch = gse::radians(0.f);
		gse::angle knee_bend = gse::radians(0.f);
		bool reachable = true;
	};

	auto solve_leg_ik(
		const gse::vec3<gse::position>& hip_world,
		const gse::vec3<gse::position>& foot_target_world,
		const gse::quat& pelvis_orientation,
		gse::length thigh_length,
		gse::length shin_length
	) -> leg_ik_result;

	auto hip_world_position(
		const state& s,
		const skeleton_refs& r,
		leg which
	) -> gse::vec3<gse::position>;
}

auto gs::locomotion::hip_world_position(const state& s, const skeleton_refs& r, const leg which) -> gse::vec3<gse::position> {
	const auto local_hip = gse::vec3<gse::displacement>(
		r.hip_offset_lateral * side_of(which),
		-r.hip_offset_below_pelvis,
		gse::meters(0.f)
	);
	return s.pelvis_position + gse::rotate_vector(s.pelvis_orientation, local_hip);
}

auto gs::locomotion::solve_leg_ik(const gse::vec3<gse::position>& hip_world, const gse::vec3<gse::position>& foot_target_world, const gse::quat& pelvis_orientation, const gse::length thigh_length, const gse::length shin_length) -> leg_ik_result {
	const auto foot_rel_world = foot_target_world - hip_world;
	const auto foot_rel_body = gse::inverse_rotate_vector(pelvis_orientation, foot_rel_world);

	const gse::length sagittal = -foot_rel_body.z();
	const gse::length drop = -foot_rel_body.y();
	const auto reach = gse::hypot(sagittal, drop);

	const auto max_reach = thigh_length + shin_length - gse::meters(0.005f);
	const auto min_reach = gse::abs(thigh_length - shin_length) + gse::meters(0.005f);
	const bool reachable = reach <= max_reach && reach >= min_reach;
	const auto reach_clamped = std::clamp(reach, min_reach, max_reach);

	const float cos_knee_interior = std::clamp(
		(thigh_length * thigh_length + shin_length * shin_length - reach_clamped * reach_clamped) /
			(2.f * thigh_length * shin_length),
		-1.f,
		1.f
	);
	const auto knee_interior = gse::acos(cos_knee_interior);
	const auto knee_bend = knee_interior - gse::radians(std::numbers::pi_v<float>);

	const float cos_phi = gse::cos(knee_bend);
	const float sin_phi = gse::sin(knee_bend);
	const auto arm_long = thigh_length + shin_length * cos_phi;
	const auto arm_perp = shin_length * sin_phi;

	const auto alpha = gse::atan2(sagittal, drop);
	const auto beta = gse::atan2(arm_perp, arm_long);
	const auto hip_pitch = alpha - beta;

	return {
		.hip_pitch = hip_pitch,
		.knee_bend = knee_bend,
		.reachable = reachable,
	};
}
