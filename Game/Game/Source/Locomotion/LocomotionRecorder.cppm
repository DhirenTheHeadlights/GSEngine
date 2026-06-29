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

	struct bone_sample {
		gse::vec3<gse::position> position;
		gse::quat orientation;
		gse::vec3<gse::velocity> velocity;
		gse::vec3<gse::angular_velocity> angular_velocity;
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

	struct reference_frame {
		observation obs;
		reference_kinematics kin;
		std::vector<bone_sample> bones;
	};

	struct reference_clip {
		std::vector<reference_frame> frames;
		gse::time dt = gse::seconds(0.f);
		std::uint32_t bone_count = 0;
	};

	auto load_reference_clip(
		std::string_view path
	) -> std::optional<reference_clip>;
}

export namespace gs::locomotion::recorder {
	struct [[= gse::system_state<"Locomotion Recorder">{}]] data {
		float phi = 0.f;
		std::unique_ptr<std::ofstream> out;
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		const recorder_config& config,
		gse::read<skeleton_refs> refs,
		gse::read<state> states,
		gse::read<intent> intents,
		gse::read<gait> gaits,
		gse::read<gse::physics::joint_drive_component> drives,
		gse::read<gse::physics::motor_component> motors,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

namespace gs::locomotion {
	struct record_header {
		std::uint32_t magic = 0x4C4F434Fu;
		std::uint32_t version = 1;
		std::uint32_t obs_dim = 0;
		std::uint32_t act_dim = 0;
		std::uint64_t obs_layout_hash = 0;
		std::uint64_t act_layout_hash = 0;
		gse::time fixed_dt = gse::seconds(0.f);
		std::uint32_t controlled_joint_count = 10;
		std::uint32_t bone_count = 0;
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

	auto update_phi(
		recorder::data& d,
		const gait& g
	) -> void;

	auto open_recording(
		recorder::data& d,
		const recorder_config& config,
		std::uint32_t bone_count
	) -> void;

	auto capture_bones(
		const skeleton_refs& r,
		gse::read<gse::physics::transform_component>& transforms,
		gse::read<gse::physics::motion_component>& motions,
		std::vector<bone_sample>& out
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

auto gs::locomotion::open_recording(recorder::data& d, const recorder_config& config, const std::uint32_t bone_count) -> void {
	d.out = std::make_unique<std::ofstream>(config.path, std::ios::binary | std::ios::trunc);
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
		.bone_count = bone_count,
	};
	write_pod(*d.out, header);
}

auto gs::locomotion::capture_bones(const skeleton_refs& r, gse::read<gse::physics::transform_component>& transforms, gse::read<gse::physics::motion_component>& motions, std::vector<bone_sample>& out) -> void {
	out.clear();
	for (const auto bone_id : r.all_bone_ids) {
		auto sample = bone_sample{};
		if (const auto* tc = transforms.find(bone_id)) {
			sample.position = tc->position;
			sample.orientation = tc->orientation;
		}
		if (const auto* mc = motions.find(bone_id)) {
			sample.velocity = mc->current_velocity;
			sample.angular_velocity = mc->angular_velocity;
		}
		out.push_back(sample);
	}
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

auto gs::locomotion::load_reference_clip(const std::string_view path) -> std::optional<reference_clip> {
	auto in = std::ifstream(std::string(path), std::ios::binary);
	if (!in.is_open()) {
		return std::nullopt;
	}
	auto header = record_header{};
	if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) {
		return std::nullopt;
	}
	if (header.magic != 0x4C4F434Fu || header.version != 1) {
		return std::nullopt;
	}
	if (header.obs_layout_hash != gse::layout_hash<observation>() || header.act_layout_hash != gse::layout_hash<action>()) {
		return std::nullopt;
	}

	auto clip = reference_clip{};
	clip.dt = header.fixed_dt;
	clip.bone_count = header.bone_count;

	const auto read_pod = [&in](auto& value) -> bool {
		return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(value)));
	};

	while (true) {
		auto frame = reference_frame{};
		auto act = action{};
		auto diag = actuation_diagnostics{};
		if (!read_pod(frame.obs)) {
			break;
		}
		if (!read_pod(act) || !read_pod(diag) || !read_pod(frame.kin)) {
			break;
		}
		frame.bones.resize(header.bone_count);
		auto bones_ok = true;
		for (std::uint32_t b = 0; b < header.bone_count; ++b) {
			if (!read_pod(frame.bones[b])) {
				bones_ok = false;
				break;
			}
		}
		if (!bones_ok) {
			break;
		}
		clip.frames.push_back(std::move(frame));
	}

	if (clip.frames.empty()) {
		return std::nullopt;
	}
	return clip;
}

auto gs::locomotion::recorder::run(data& d, const recorder_config& config, gse::read<skeleton_refs> refs, gse::read<state> states, gse::read<intent> intents, gse::read<gait> gaits, gse::read<gse::physics::joint_drive_component> drives, gse::read<gse::physics::motor_component> motors, gse::read<gse::physics::transform_component> transforms, gse::read<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (!config.enabled) {
		return {};
	}

	auto bones = std::vector<bone_sample>{};
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
			open_recording(d, config, static_cast<std::uint32_t>(r->all_bone_ids.size()));
			if (!d.out) {
				continue;
			}
		}

		update_phi(d, *g);

		const auto obs = observe(*s, *it, *g, d.phi);
		const auto act = action_from_drives(*r, drives);
		const auto diag = capture_actuation(*r, drives, motors, owner);
		const auto ref_kin = capture_reference_kinematics(*s, d.phi);
		capture_bones(*r, transforms, motions, bones);

		write_pod(*d.out, obs);
		write_pod(*d.out, act);
		write_pod(*d.out, diag);
		write_pod(*d.out, ref_kin);
		for (const auto& bone : bones) {
			write_pod(*d.out, bone);
		}
	}

	return {};
}
