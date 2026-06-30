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

	struct amp_feature_layout {
		gse::angle hip_l;
		gse::angle knee_l;
		gse::angle ankle_l;
		gse::angle hip_r;
		gse::angle knee_r;
		gse::angle ankle_r;
		gse::angular_velocity hip_rate_l;
		gse::angular_velocity knee_rate_l;
		gse::angular_velocity ankle_rate_l;
		gse::angular_velocity hip_rate_r;
		gse::angular_velocity knee_rate_r;
		gse::angular_velocity ankle_rate_r;
		gse::vec3<gse::displacement> foot_l_local;
		gse::vec3<gse::displacement> foot_r_local;
		gse::displacement pelvis_height;
		gse::vec3f gravity_body;
		gse::vec3<gse::angular_velocity> angular_body;
	};
	static_assert(sizeof(amp_feature_layout) % sizeof(float) == 0);

	constexpr std::size_t amp_feature_dim = sizeof(amp_feature_layout) / sizeof(float);

	struct amp_dataset {
		std::vector<float> features;
		std::vector<std::uint32_t> transition_starts;
		std::size_t frame_count = 0;
	};

	auto amp_features_reference(
		const reference_frame& frame,
		const skeleton_refs& refs,
		std::span<float, amp_feature_dim> out
	) -> void;

	auto amp_features_state(
		const state& s,
		const gse::vec3<gse::angular_velocity>& pelvis_angular_velocity,
		std::span<float, amp_feature_dim> out
	) -> void;

	auto build_amp_dataset(
		std::span<const reference_clip> clips,
		const skeleton_refs& refs
	) -> amp_dataset;

	auto sample_amp_transition(
		const amp_dataset& dataset,
		std::mt19937& rng,
		std::span<float> out
	) -> void;

	auto amp_selftest() -> bool;
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

	auto pack_amp_features(
		const std::array<gse::angle, 6>& joint_angles,
		const std::array<gse::angular_velocity, 6>& joint_rates,
		const gse::vec3<gse::position>& pelvis_position,
		const gse::quat& pelvis_orientation,
		const gse::vec3<gse::angular_velocity>& pelvis_angular_velocity,
		const gse::vec3<gse::position>& foot_l,
		const gse::vec3<gse::position>& foot_r,
		gse::displacement pelvis_height,
		std::span<float, amp_feature_dim> out
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

auto gs::locomotion::pack_amp_features(const std::array<gse::angle, 6>& joint_angles, const std::array<gse::angular_velocity, 6>& joint_rates, const gse::vec3<gse::position>& pelvis_position, const gse::quat& pelvis_orientation, const gse::vec3<gse::angular_velocity>& pelvis_angular_velocity, const gse::vec3<gse::position>& foot_l, const gse::vec3<gse::position>& foot_r, const gse::displacement pelvis_height, const std::span<float, amp_feature_dim> out) -> void {
	const auto features = amp_feature_layout{
		.hip_l = joint_angles[0],
		.knee_l = joint_angles[1],
		.ankle_l = joint_angles[2],
		.hip_r = joint_angles[3],
		.knee_r = joint_angles[4],
		.ankle_r = joint_angles[5],
		.hip_rate_l = joint_rates[0],
		.knee_rate_l = joint_rates[1],
		.ankle_rate_l = joint_rates[2],
		.hip_rate_r = joint_rates[3],
		.knee_rate_r = joint_rates[4],
		.ankle_rate_r = joint_rates[5],
		.foot_l_local = gse::inverse_rotate_vector(pelvis_orientation, foot_l - pelvis_position),
		.foot_r_local = gse::inverse_rotate_vector(pelvis_orientation, foot_r - pelvis_position),
		.pelvis_height = pelvis_height,
		.gravity_body = gse::inverse_rotate_vector(pelvis_orientation, gse::vec3f(0.f, 1.f, 0.f)),
		.angular_body = gse::inverse_rotate_vector(pelvis_orientation, pelvis_angular_velocity),
	};
	gse::memcpy(out, features);
}

auto gs::locomotion::amp_features_reference(const reference_frame& frame, const skeleton_refs& refs, const std::span<float, amp_feature_dim> out) -> void {
	const auto angles = std::array<gse::angle, 6>{
		frame.kin.hip_angle_l, frame.kin.knee_angle_l, frame.kin.ankle_angle_l,
		frame.kin.hip_angle_r, frame.kin.knee_angle_r, frame.kin.ankle_angle_r,
	};
	const auto rates = std::array<gse::angular_velocity, 6>{
		frame.obs.hip_rate_l, frame.obs.knee_rate_l, frame.obs.ankle_rate_l,
		frame.obs.hip_rate_r, frame.obs.knee_rate_r, frame.obs.ankle_rate_r,
	};
	const auto bone_index = [&refs](const gse::id id) -> std::size_t {
		for (std::size_t i = 0; i < refs.all_bone_ids.size(); ++i) {
			if (refs.all_bone_ids[i] == id) {
				return i;
			}
		}
		return 0;
	};
	const auto pelvis = frame.bones.empty() ? frame.kin.pelvis_position : frame.bones[0].position;
	const auto fl = bone_index(refs.foot_l_id);
	const auto fr = bone_index(refs.foot_r_id);
	const auto foot_l = fl < frame.bones.size() ? frame.bones[fl].position : pelvis;
	const auto foot_r = fr < frame.bones.size() ? frame.bones[fr].position : pelvis;
	const auto pelvis_angular_velocity = frame.bones.empty() ? gse::vec3<gse::angular_velocity>{} : frame.bones[0].angular_velocity;
	pack_amp_features(angles, rates, pelvis, frame.kin.pelvis_orientation, pelvis_angular_velocity, foot_l, foot_r, frame.obs.pelvis_height, out);
}

auto gs::locomotion::amp_features_state(const state& s, const gse::vec3<gse::angular_velocity>& pelvis_angular_velocity, const std::span<float, amp_feature_dim> out) -> void {
	const auto angles = std::array<gse::angle, 6>{
		s.hip_angle_l, s.knee_angle_l, s.ankle_angle_l,
		s.hip_angle_r, s.knee_angle_r, s.ankle_angle_r,
	};
	const auto rates = std::array<gse::angular_velocity, 6>{
		s.hip_rate_l, s.knee_rate_l, s.ankle_rate_l,
		s.hip_rate_r, s.knee_rate_r, s.ankle_rate_r,
	};
	const auto pelvis_height = s.pelvis_position.y() - s.support_center.y();
	pack_amp_features(angles, rates, s.pelvis_position, s.pelvis_orientation, pelvis_angular_velocity, s.foot_position_l, s.foot_position_r, pelvis_height, out);
}

auto gs::locomotion::build_amp_dataset(const std::span<const reference_clip> clips, const skeleton_refs& refs) -> amp_dataset {
	auto dataset = amp_dataset{};
	auto frame_features = std::array<float, amp_feature_dim>{};
	const auto jump_limit = gse::meters(0.5f);
	for (const auto& clip : clips) {
		const auto base = dataset.frame_count;
		for (const auto& frame : clip.frames) {
			amp_features_reference(frame, refs, frame_features);
			dataset.features.insert(dataset.features.end(), frame_features.begin(), frame_features.end());
			++dataset.frame_count;
		}
		for (std::size_t i = 1; i < clip.frames.size(); ++i) {
			const auto jump = gse::magnitude(clip.frames[i].kin.pelvis_position - clip.frames[i - 1].kin.pelvis_position);
			if (jump < jump_limit) {
				dataset.transition_starts.push_back(static_cast<std::uint32_t>(base + i - 1));
			}
		}
	}
	return dataset;
}

auto gs::locomotion::sample_amp_transition(const amp_dataset& dataset, std::mt19937& rng, const std::span<float> out) -> void {
	if (dataset.transition_starts.empty() || out.size() < 2 * amp_feature_dim) {
		std::ranges::fill(out, 0.0f);
		return;
	}
	auto pick = std::uniform_int_distribution<std::size_t>(0, dataset.transition_starts.size() - 1);
	const auto start = static_cast<std::size_t>(dataset.transition_starts[pick(rng)]);
	const auto* a = dataset.features.data() + start * amp_feature_dim;
	const auto* b = dataset.features.data() + (start + 1) * amp_feature_dim;
	std::memcpy(out.data(), a, amp_feature_dim * sizeof(float));
	std::memcpy(out.data() + amp_feature_dim, b, amp_feature_dim * sizeof(float));
}

auto gs::locomotion::amp_selftest() -> bool {
	auto ok = true;

	auto refs = skeleton_refs{};
	const auto pelvis_id = gse::generate_temp_id(900001u);
	const auto foot_l_id = gse::generate_temp_id(900002u);
	const auto foot_r_id = gse::generate_temp_id(900003u);
	refs.pelvis_id = pelvis_id;
	refs.foot_l_id = foot_l_id;
	refs.foot_r_id = foot_r_id;
	refs.all_bone_ids = { pelvis_id, foot_l_id, foot_r_id };

	const auto support_y = gse::position(0.05f);
	const auto make_frame = [&](const int k) -> reference_frame {
		auto frame = reference_frame{};
		const auto fk = static_cast<float>(k);
		const auto pelvis = gse::vec3<gse::position>(gse::position(0.02f * fk), gse::position(0.95f + 0.001f * fk), gse::position(-0.06f * fk));
		const auto orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), gse::radians(0.08f * fk));
		const auto foot_l = pelvis + gse::vec3<gse::displacement>(-0.10f, -0.90f, 0.05f + 0.01f * fk);
		const auto foot_r = pelvis + gse::vec3<gse::displacement>(0.10f, -0.90f, -0.05f);
		const auto angular = gse::vec3<gse::angular_velocity>(gse::radians_per_second(0.01f * fk), gse::radians_per_second(0.02f * fk), gse::radians_per_second(-0.03f * fk));

		frame.kin.pelvis_position = pelvis;
		frame.kin.pelvis_orientation = orientation;
		frame.kin.hip_angle_l = gse::radians(0.10f * fk);
		frame.kin.knee_angle_l = gse::radians(-0.20f * fk);
		frame.kin.ankle_angle_l = gse::radians(0.05f * fk);
		frame.kin.hip_angle_r = gse::radians(-0.10f * fk);
		frame.kin.knee_angle_r = gse::radians(-0.15f * fk);
		frame.kin.ankle_angle_r = gse::radians(0.03f * fk);
		frame.kin.phi = 0.10f * fk;

		frame.obs.pelvis_height = pelvis.y() - support_y;
		frame.obs.hip_rate_l = gse::radians_per_second(0.50f * fk);
		frame.obs.knee_rate_l = gse::radians_per_second(-0.30f * fk);
		frame.obs.ankle_rate_l = gse::radians_per_second(0.20f * fk);
		frame.obs.hip_rate_r = gse::radians_per_second(-0.50f * fk);
		frame.obs.knee_rate_r = gse::radians_per_second(-0.25f * fk);
		frame.obs.ankle_rate_r = gse::radians_per_second(0.10f * fk);

		auto pelvis_bone = bone_sample{};
		pelvis_bone.position = pelvis;
		pelvis_bone.orientation = orientation;
		pelvis_bone.angular_velocity = angular;
		auto foot_l_bone = bone_sample{};
		foot_l_bone.position = foot_l;
		auto foot_r_bone = bone_sample{};
		foot_r_bone.position = foot_r;
		frame.bones = { pelvis_bone, foot_l_bone, foot_r_bone };
		return frame;
	};

	auto clip = reference_clip{};
	clip.dt = gse::seconds(1.0f / 60.0f);
	clip.bone_count = 3;
	constexpr auto frame_count = std::size_t{ 6 };
	for (std::size_t k = 0; k < frame_count; ++k) {
		clip.frames.push_back(make_frame(static_cast<int>(k)));
	}

	const auto dataset = build_amp_dataset(std::span<const reference_clip>(&clip, 1), refs);

	const auto close = [](const std::span<const float> a, const std::span<const float> b, const float tol) -> bool {
		if (a.size() != b.size()) {
			return false;
		}
		for (std::size_t i = 0; i < a.size(); ++i) {
			if (std::abs(a[i] - b[i]) > tol) {
				return false;
			}
		}
		return true;
	};

	if (amp_feature_dim != 25) {
		gse::log::println("amp_selftest: FAIL amp_feature_dim={} expected 25", amp_feature_dim);
		ok = false;
	}
	if (dataset.frame_count != frame_count || dataset.features.size() != frame_count * amp_feature_dim) {
		gse::log::println("amp_selftest: FAIL dataset frames={} features={}", dataset.frame_count, dataset.features.size());
		ok = false;
	}
	if (dataset.transition_starts.size() != frame_count - 1) {
		gse::log::println("amp_selftest: FAIL transition_starts={} expected {}", dataset.transition_starts.size(), frame_count - 1);
		ok = false;
	}

	auto recomputed = std::array<float, amp_feature_dim>{};
	amp_features_reference(clip.frames[2], refs, recomputed);
	if (dataset.features.size() >= 3 * amp_feature_dim && !close(recomputed, std::span<const float>(dataset.features).subspan(2 * amp_feature_dim, amp_feature_dim), 1e-6f)) {
		gse::log::println("amp_selftest: FAIL dataset feature slice mismatch at frame 2");
		ok = false;
	}

	auto rng = std::mt19937(4242u);
	auto pair = std::array<float, 2 * amp_feature_dim>{};
	auto consecutive = true;
	for (auto t = 0; t < 32 && consecutive; ++t) {
		sample_amp_transition(dataset, rng, pair);
		auto matched = false;
		for (std::size_t i = 0; i + 1 < dataset.frame_count; ++i) {
			const auto fa = std::span<const float>(dataset.features).subspan(i * amp_feature_dim, amp_feature_dim);
			const auto fb = std::span<const float>(dataset.features).subspan((i + 1) * amp_feature_dim, amp_feature_dim);
			if (close(std::span<const float>(pair).first(amp_feature_dim), fa, 1e-6f) && close(std::span<const float>(pair).last(amp_feature_dim), fb, 1e-6f)) {
				matched = true;
				break;
			}
		}
		consecutive = matched;
	}
	if (!consecutive) {
		gse::log::println("amp_selftest: FAIL sampled transition is not a consecutive dataset pair");
		ok = false;
	}

	const auto& cf = clip.frames[3];
	auto ref_feat = std::array<float, amp_feature_dim>{};
	amp_features_reference(cf, refs, ref_feat);

	auto s = state{};
	s.pelvis_position = cf.bones[0].position;
	s.pelvis_orientation = cf.kin.pelvis_orientation;
	s.foot_position_l = cf.bones[1].position;
	s.foot_position_r = cf.bones[2].position;
	s.support_center = gse::vec3<gse::position>(gse::position(0.f), support_y, gse::position(0.f));
	s.hip_angle_l = cf.kin.hip_angle_l;
	s.knee_angle_l = cf.kin.knee_angle_l;
	s.ankle_angle_l = cf.kin.ankle_angle_l;
	s.hip_angle_r = cf.kin.hip_angle_r;
	s.knee_angle_r = cf.kin.knee_angle_r;
	s.ankle_angle_r = cf.kin.ankle_angle_r;
	s.hip_rate_l = cf.obs.hip_rate_l;
	s.knee_rate_l = cf.obs.knee_rate_l;
	s.ankle_rate_l = cf.obs.ankle_rate_l;
	s.hip_rate_r = cf.obs.hip_rate_r;
	s.knee_rate_r = cf.obs.knee_rate_r;
	s.ankle_rate_r = cf.obs.ankle_rate_r;

	auto state_feat = std::array<float, amp_feature_dim>{};
	amp_features_state(s, cf.bones[0].angular_velocity, state_feat);
	if (!close(ref_feat, state_feat, 1e-4f)) {
		gse::log::println("amp_selftest: FAIL reference/state feature mismatch (offline != online)");
		ok = false;
	}

	const auto yaw = gse::quat(gse::vec3f(0.f, 1.f, 0.f), gse::radians(1.3f));
	auto rotated = s;
	rotated.pelvis_orientation = yaw * s.pelvis_orientation;
	rotated.foot_position_l = s.pelvis_position + gse::rotate_vector(yaw, s.foot_position_l - s.pelvis_position);
	rotated.foot_position_r = s.pelvis_position + gse::rotate_vector(yaw, s.foot_position_r - s.pelvis_position);
	auto rotated_feat = std::array<float, amp_feature_dim>{};
	amp_features_state(rotated, gse::rotate_vector(yaw, cf.bones[0].angular_velocity), rotated_feat);
	if (!close(state_feat, rotated_feat, 1e-4f)) {
		gse::log::println("amp_selftest: FAIL features not yaw-invariant");
		ok = false;
	}

	gse::log::println("amp_selftest: {} feature_dim={} frames={} transitions={}", ok ? "PASS" : "FAIL", amp_feature_dim, dataset.frame_count, dataset.transition_starts.size());
	return ok;
}
