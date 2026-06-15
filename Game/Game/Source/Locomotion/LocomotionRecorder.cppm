export module gs:locomotion_recorder;

import std;
import gse;

import :locomotion_types;
import :locomotion_mdp;

export namespace gs::locomotion {
	struct recorder_config {
		bool enabled = false;
		std::string path;
	};

	struct recorder {
		struct data {
			bool enabled = false;
			std::string path;
			float phi = 0.f;
			std::unique_ptr<std::ofstream> out;

			explicit data(recorder_config cfg = {});
		};

		static auto run(
			data& d,
			gse::read<skeleton_refs> refs,
			gse::read<state> states,
			gse::read<intent> intents,
			gse::read<gait> gaits,
			gse::read<gse::physics::joint_drive_component> drives,
			gse::read<gse::physics::motor_component> motors
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	struct record_header {
		std::uint32_t magic = 0x4C4F434Fu;
		std::uint32_t version = 0;
		std::uint32_t obs_dim = 0;
		std::uint32_t act_dim = 0;
		std::uint64_t obs_layout_hash = 0;
		std::uint64_t act_layout_hash = 0;
		gse::time fixed_dt = gse::seconds(0.f);
		std::uint32_t controlled_joint_count = 10;
	};

	struct actuation_diagnostics {
		gse::vec3<gse::angular_stiffness> hip_stiffness_l;
		gse::torque hip_torque_l;
		gse::angular_stiffness knee_stiffness_l;
		gse::torque knee_torque_l;
		gse::angular_stiffness ankle_stiffness_l;
		gse::torque ankle_torque_l;
		gse::vec3<gse::angular_stiffness> hip_stiffness_r;
		gse::torque hip_torque_r;
		gse::angular_stiffness knee_stiffness_r;
		gse::torque knee_torque_r;
		gse::angular_stiffness ankle_stiffness_r;
		gse::torque ankle_torque_r;
		gse::vec3<gse::velocity> pelvis_motor_target;
		gse::force pelvis_motor_max_force;
	};

	struct reference_kinematics {
		gse::vec3<gse::position> pelvis_position;
		gse::quat pelvis_orientation;
		gse::vec3<gse::velocity> pelvis_velocity;
		gse::angle hip_angle_l;
		gse::angle knee_angle_l;
		gse::angle ankle_angle_l;
		gse::angle hip_angle_r;
		gse::angle knee_angle_r;
		gse::angle ankle_angle_r;
		float phi = 0.f;
		bool foot_grounded_l = false;
		bool foot_grounded_r = false;
	};

	auto update_phi(
		recorder::data& d,
		const gait& g
	) -> void;

	auto open_recording(
		recorder::data& d
	) -> void;

	auto capture_actuation(
		const skeleton_refs& r,
		gse::read<gse::physics::joint_drive_component>& drives,
		gse::read<gse::physics::motor_component>& motors,
		gse::id owner
	) -> actuation_diagnostics;

	auto capture_reference_kinematics(
		const state& s,
		float phi
	) -> reference_kinematics;

	template <typename T>
	auto write_pod(
		std::ofstream& out,
		const T& value
	) -> void;
}

gs::locomotion::recorder::data::data(recorder_config cfg)
	: enabled(cfg.enabled), path(std::move(cfg.path)) {}

auto gs::locomotion::update_phi(recorder::data& d, const gait& g) -> void {
	if (g.current != phase::swing) {
		return;
	}
	const auto progress = phase_progress(g);
	if (g.swing_leg == leg::left) {
		d.phi = progress * 0.5f;
	}
	else {
		d.phi = 0.5f + progress * 0.5f;
	}
}

auto gs::locomotion::open_recording(recorder::data& d) -> void {
	d.out = std::make_unique<std::ofstream>(d.path, std::ios::binary | std::ios::trunc);
	if (!d.out->is_open()) {
		d.out.reset();
		return;
	}
	const auto header = record_header{
		.obs_dim = static_cast<std::uint32_t>(scalar_count<observation>()),
		.act_dim = static_cast<std::uint32_t>(scalar_count<action>()),
		.obs_layout_hash = gse::layout_hash<observation>(),
		.act_layout_hash = gse::layout_hash<action>(),
		.fixed_dt = gse::system_clock::fixed_dt<gse::time>(),
	};
	write_pod(*d.out, header);
}

auto gs::locomotion::capture_actuation(const skeleton_refs& r, gse::read<gse::physics::joint_drive_component>& drives, gse::read<gse::physics::motor_component>& motors, const gse::id owner) -> actuation_diagnostics {
	auto diag = actuation_diagnostics{};
	if (const auto* drv = drives.find(r.hip_l_joint_id)) {
		diag.hip_stiffness_l = drv->stiffness;
		diag.hip_torque_l = drv->max_torque;
	}
	if (const auto* drv = drives.find(r.knee_l_joint_id)) {
		diag.knee_stiffness_l = drv->stiffness.x();
		diag.knee_torque_l = drv->max_torque;
	}
	if (const auto* drv = drives.find(r.ankle_l_joint_id)) {
		diag.ankle_stiffness_l = drv->stiffness.x();
		diag.ankle_torque_l = drv->max_torque;
	}
	if (const auto* drv = drives.find(r.hip_r_joint_id)) {
		diag.hip_stiffness_r = drv->stiffness;
		diag.hip_torque_r = drv->max_torque;
	}
	if (const auto* drv = drives.find(r.knee_r_joint_id)) {
		diag.knee_stiffness_r = drv->stiffness.x();
		diag.knee_torque_r = drv->max_torque;
	}
	if (const auto* drv = drives.find(r.ankle_r_joint_id)) {
		diag.ankle_stiffness_r = drv->stiffness.x();
		diag.ankle_torque_r = drv->max_torque;
	}
	if (const auto* mot = motors.find(owner)) {
		diag.pelvis_motor_target = mot->velocity_drive_target;
		diag.pelvis_motor_max_force = mot->max_force;
	}
	return diag;
}

auto gs::locomotion::capture_reference_kinematics(const state& s, const float phi) -> reference_kinematics {
	return {
		.pelvis_position = s.pelvis_position,
		.pelvis_orientation = s.pelvis_orientation,
		.pelvis_velocity = s.pelvis_velocity,
		.hip_angle_l = s.hip_angle_l,
		.knee_angle_l = s.knee_angle_l,
		.ankle_angle_l = s.ankle_angle_l,
		.hip_angle_r = s.hip_angle_r,
		.knee_angle_r = s.knee_angle_r,
		.ankle_angle_r = s.ankle_angle_r,
		.phi = phi,
		.foot_grounded_l = s.foot_grounded_l,
		.foot_grounded_r = s.foot_grounded_r,
	};
}

template <typename T>
auto gs::locomotion::write_pod(std::ofstream& out, const T& value) -> void {
	out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

auto gs::locomotion::recorder::run(data& d, gse::read<skeleton_refs> refs, gse::read<state> states, gse::read<intent> intents, gse::read<gait> gaits, gse::read<gse::physics::joint_drive_component> drives, gse::read<gse::physics::motor_component> motors) -> gse::async::task<> {
	if (!d.enabled) {
		return {};
	}

	const auto owner_ids = refs.owner_ids();
	for (std::size_t i = 0; i < owner_ids.size(); ++i) {
		const auto owner = owner_ids[i];
		const auto* r = refs.find(owner);
		const auto* s = states.find(owner);
		const auto* it = intents.find(owner);
		const auto* g = gaits.find(owner);
		if (!r || !s || !it || !g || !s->valid) {
			continue;
		}

		if (!d.out) {
			open_recording(d);
			if (!d.out) {
				continue;
			}
		}

		update_phi(d, *g);

		const auto obs = observe(*s, *it, *g, d.phi);
		const auto act = action_from_drives(*r, drives);
		const auto diag = capture_actuation(*r, drives, motors, owner);
		const auto ref_kin = capture_reference_kinematics(*s, d.phi);

		write_pod(*d.out, obs);
		write_pod(*d.out, act);
		write_pod(*d.out, diag);
		write_pod(*d.out, ref_kin);
	}

	return {};
}
