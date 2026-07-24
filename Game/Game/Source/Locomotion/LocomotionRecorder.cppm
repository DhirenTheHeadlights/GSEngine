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

	enum class bvh_channel : std::uint8_t { x_pos, y_pos, z_pos, x_rot, y_rot, z_rot };

	struct bvh_joint {
		std::string name;
		int parent = -1;
		gse::vec3f offset;
		std::vector<bvh_channel> channels;
		int channel_base = 0;
		bool end_site = false;
	};

	struct bvh_clip {
		std::vector<bvh_joint> joints;
		int channels_per_frame = 0;
		float frame_time = 0.f;
		int frame_count = 0;
		std::vector<float> motion;
	};

	auto parse_bvh(std::string_view path) -> std::optional<bvh_clip>;
	auto find_bvh_joint(const bvh_clip& clip, std::string_view name) -> int;
	auto bvh_clip_info(std::string_view path) -> bool;
	auto import_mocap(std::string_view bvh_path, std::string_view out_path) -> bool;
	auto clip_kinematics_stats(std::string_view path) -> bool;

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

static auto bvh_channel_from(const std::string_view t) -> gs::locomotion::bvh_channel {
	using c = gs::locomotion::bvh_channel;
	if (t == "Xposition") return c::x_pos;
	if (t == "Yposition") return c::y_pos;
	if (t == "Zposition") return c::z_pos;
	if (t == "Xrotation") return c::x_rot;
	if (t == "Yrotation") return c::y_rot;
	return c::z_rot;
}

static auto bvh_is_space(const char ch) -> bool {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

auto gs::locomotion::parse_bvh(const std::string_view path) -> std::optional<bvh_clip> {
	auto in = std::ifstream(std::string(path), std::ios::binary);
	if (!in.is_open()) {
		return std::nullopt;
	}
	const auto content = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

	auto tokens = std::vector<std::string>{};
	for (std::size_t i = 0; i < content.size();) {
		while (i < content.size() && bvh_is_space(content[i])) {
			++i;
		}
		auto j = i;
		while (j < content.size() && !bvh_is_space(content[j])) {
			++j;
		}
		if (j > i) {
			tokens.push_back(content.substr(i, j - i));
		}
		i = j;
	}

	auto clip = bvh_clip{};
	std::size_t t = 0;
	if (t >= tokens.size() || tokens[t] != "HIERARCHY") {
		return std::nullopt;
	}
	++t;

	auto parent_stack = std::vector<int>{};
	int current = -1;
	while (t < tokens.size() && tokens[t] != "MOTION") {
		const auto& tok = tokens[t];
		if (tok == "ROOT" || tok == "JOINT") {
			if (t + 1 >= tokens.size()) return std::nullopt;
			auto j = bvh_joint{};
			j.name = tokens[t + 1];
			j.parent = current;
			clip.joints.push_back(std::move(j));
			parent_stack.push_back(current);
			current = static_cast<int>(clip.joints.size()) - 1;
			t += 2;
			if (t >= tokens.size() || tokens[t] != "{") return std::nullopt;
			++t;
		}
		else if (tok == "End") {
			auto j = bvh_joint{};
			j.name = "End Site";
			j.parent = current;
			j.end_site = true;
			clip.joints.push_back(std::move(j));
			parent_stack.push_back(current);
			current = static_cast<int>(clip.joints.size()) - 1;
			t += 2;
			if (t >= tokens.size() || tokens[t] != "{") return std::nullopt;
			++t;
		}
		else if (tok == "OFFSET") {
			if (current < 0 || t + 3 >= tokens.size()) return std::nullopt;
			clip.joints[static_cast<std::size_t>(current)].offset = gse::vec3f(
				std::stof(tokens[t + 1]), std::stof(tokens[t + 2]), std::stof(tokens[t + 3]));
			t += 4;
		}
		else if (tok == "CHANNELS") {
			if (current < 0 || t + 1 >= tokens.size()) return std::nullopt;
			const int n = std::stoi(tokens[t + 1]);
			t += 2;
			auto& j = clip.joints[static_cast<std::size_t>(current)];
			j.channel_base = clip.channels_per_frame;
			for (int k = 0; k < n && t < tokens.size(); ++k) {
				j.channels.push_back(bvh_channel_from(tokens[t]));
				++t;
			}
			clip.channels_per_frame += n;
		}
		else if (tok == "}") {
			current = parent_stack.empty() ? -1 : parent_stack.back();
			if (!parent_stack.empty()) {
				parent_stack.pop_back();
			}
			++t;
		}
		else {
			++t;
		}
	}

	if (t >= tokens.size() || tokens[t] != "MOTION") {
		return std::nullopt;
	}
	++t;
	while (t < tokens.size()) {
		if (tokens[t] == "Frames:") {
			clip.frame_count = std::stoi(tokens[t + 1]);
			t += 2;
		}
		else if (tokens[t] == "Time:") {
			clip.frame_time = std::stof(tokens[t + 1]);
			t += 2;
		}
		else if (tokens[t] == "Frame") {
			++t;
		}
		else {
			break;
		}
	}

	const auto expected = static_cast<std::size_t>(clip.frame_count) * static_cast<std::size_t>(clip.channels_per_frame);
	clip.motion.reserve(expected);
	while (t < tokens.size() && clip.motion.size() < expected) {
		clip.motion.push_back(std::stof(tokens[t]));
		++t;
	}
	if (clip.joints.empty() || clip.channels_per_frame == 0 || clip.motion.size() != expected) {
		return std::nullopt;
	}
	return clip;
}

auto gs::locomotion::find_bvh_joint(const bvh_clip& clip, const std::string_view name) -> int {
	for (std::size_t i = 0; i < clip.joints.size(); ++i) {
		if (clip.joints[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

auto gs::locomotion::bvh_clip_info(const std::string_view path) -> bool {
	const auto parsed = parse_bvh(path);
	if (!parsed) {
		gse::log::println("bvh_clip_info: FAILED to parse '{}'", path);
		return false;
	}
	const auto& c = *parsed;
	auto names = std::string{};
	int root = -1;
	int named = 0;
	for (std::size_t i = 0; i < c.joints.size(); ++i) {
		if (c.joints[i].parent == -1) {
			root = static_cast<int>(i);
		}
		if (!c.joints[i].end_site) {
			if (named > 0) {
				names += ", ";
			}
			names += c.joints[i].name;
			++named;
		}
	}
	const auto fps = c.frame_time > 0.f ? 1.0f / c.frame_time : 0.f;
	auto ymin = std::numeric_limits<float>::max();
	auto ymax = std::numeric_limits<float>::lowest();
	if (root >= 0) {
		int ych = -1;
		const auto& rj = c.joints[static_cast<std::size_t>(root)];
		for (std::size_t k = 0; k < rj.channels.size(); ++k) {
			if (rj.channels[k] == bvh_channel::y_pos) {
				ych = rj.channel_base + static_cast<int>(k);
			}
		}
		if (ych >= 0) {
			for (int f = 0; f < c.frame_count; ++f) {
				const auto v = c.motion[static_cast<std::size_t>(f) * static_cast<std::size_t>(c.channels_per_frame) + static_cast<std::size_t>(ych)];
				ymin = std::min(ymin, v);
				ymax = std::max(ymax, v);
			}
		}
	}
	gse::log::println("bvh_clip_info: '{}' joints={} (named={}) channels/frame={} frames={} frame_time={:.6f} fps={:.1f} dur={:.2f}s",
		path, c.joints.size(), named, c.channels_per_frame, c.frame_count, c.frame_time, fps, static_cast<float>(c.frame_count) * c.frame_time);
	gse::log::println("bvh_clip_info: root='{}' rootY=[{:.2f}, {:.2f}] (scale hint: ~15-40 = inches, ~1-2 = metres)",
		root >= 0 ? c.joints[static_cast<std::size_t>(root)].name : std::string("?"), ymin, ymax);
	gse::log::println("bvh_clip_info: joints = {}", names);
	return true;
}

static auto import_hinge_x(const gse::quat& parent, const gse::quat& child) -> gse::angle {
	const auto theta = gse::difference_axis_angle(parent, child);
	const auto axis_world = gse::rotate_vector(parent, gse::vec3f(1.f, 0.f, 0.f));
	return -gse::dot(axis_world, theta);
}

auto gs::locomotion::import_mocap(const std::string_view bvh_path, const std::string_view out_path) -> bool {
	const auto parsed = parse_bvh(bvh_path);
	if (!parsed) {
		gse::log::println("import_mocap: FAILED to parse '{}'", bvh_path);
		return false;
	}
	const auto& bvh = *parsed;

	constexpr float scale = 0.056444f;
	constexpr float deg2rad = std::numbers::pi_v<float> / 180.f;
	const auto stride = std::max(1, static_cast<int>(std::lround(1.0 / (static_cast<double>(bvh.frame_time) * 60.0))));

	const std::array<std::string_view, 17> rig_to_mocap = {
		"Hips", "Spine1", "Head",
		"LeftArm", "LeftForeArm", "LeftHand",
		"RightArm", "RightForeArm", "RightHand",
		"LeftUpLeg", "LeftLeg", "LeftFoot",
		"RightUpLeg", "RightLeg", "RightFoot",
		"LeftToeBase", "RightToeBase",
	};
	auto mocap_idx = std::array<int, 17>{};
	for (std::size_t i = 0; i < 17; ++i) {
		mocap_idx[i] = find_bvh_joint(bvh, rig_to_mocap[i]);
		if (mocap_idx[i] < 0) {
			gse::log::println("import_mocap: joint '{}' not found in BVH", rig_to_mocap[i]);
			return false;
		}
	}

	auto out = std::ofstream(std::string(out_path), std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		gse::log::println("import_mocap: cannot open output '{}'", out_path);
		return false;
	}
	const auto header = record_header{
		.obs_dim = static_cast<std::uint32_t>(scalar_count<observation>()),
		.act_dim = static_cast<std::uint32_t>(scalar_count<action>()),
		.obs_layout_hash = gse::layout_hash<observation>(),
		.act_layout_hash = gse::layout_hash<action>(),
		.fixed_dt = gse::seconds(1.f / 60.f),
		.bone_count = 17,
	};
	write_pod(out, header);

	const int njoints = static_cast<int>(bvh.joints.size());
	auto world_orient = std::vector<gse::quat>(static_cast<std::size_t>(njoints));
	auto world_pos = std::vector<gse::vec3f>(static_cast<std::size_t>(njoints));

	const auto dt = gse::seconds(1.f / 60.f);
	const float dt_s = 1.f / 60.f;
	auto have_prev = false;
	auto prev_pos = std::array<gse::vec3f, 17>{};
	auto prev_q = std::array<gse::quat, 17>{};
	auto prev_angles = std::array<gse::angle, 6>{};
	int frames_written = 0;

	for (int f = 0; f < bvh.frame_count; f += stride) {
		const auto row = static_cast<std::size_t>(f) * static_cast<std::size_t>(bvh.channels_per_frame);
		for (int j = 0; j < njoints; ++j) {
			const auto& joint = bvh.joints[static_cast<std::size_t>(j)];
			auto local_rot = gse::quat(1.f, 0.f, 0.f, 0.f);
			auto root_translation = gse::vec3f(0.f, 0.f, 0.f);
			for (std::size_t k = 0; k < joint.channels.size(); ++k) {
				const float v = bvh.motion[row + static_cast<std::size_t>(joint.channel_base) + k];
				switch (joint.channels[k]) {
					case bvh_channel::x_pos: root_translation = gse::vec3f(v, root_translation.y(), root_translation.z()); break;
					case bvh_channel::y_pos: root_translation = gse::vec3f(root_translation.x(), v, root_translation.z()); break;
					case bvh_channel::z_pos: root_translation = gse::vec3f(root_translation.x(), root_translation.y(), v); break;
					case bvh_channel::x_rot: local_rot = local_rot * gse::quat(gse::vec3f(1.f, 0.f, 0.f), gse::radians(v * deg2rad)); break;
					case bvh_channel::y_rot: local_rot = local_rot * gse::quat(gse::vec3f(0.f, 1.f, 0.f), gse::radians(v * deg2rad)); break;
					case bvh_channel::z_rot: local_rot = local_rot * gse::quat(gse::vec3f(0.f, 0.f, 1.f), gse::radians(v * deg2rad)); break;
				}
			}
			if (joint.parent < 0) {
				world_orient[static_cast<std::size_t>(j)] = local_rot;
				world_pos[static_cast<std::size_t>(j)] = joint.offset + root_translation;
			}
			else {
				const auto p = static_cast<std::size_t>(joint.parent);
				world_orient[static_cast<std::size_t>(j)] = world_orient[p] * local_rot;
				world_pos[static_cast<std::size_t>(j)] = world_pos[p] + gse::rotate_vector(world_orient[p], joint.offset);
			}
		}

		auto bones = std::array<bone_sample, 17>{};
		auto cur_pos = std::array<gse::vec3f, 17>{};
		auto cur_q = std::array<gse::quat, 17>{};
		for (std::size_t i = 0; i < 17; ++i) {
			const auto mj = static_cast<std::size_t>(mocap_idx[i]);
			const auto q = world_orient[mj];
			const auto raw = world_pos[mj];
			const auto pm = gse::vec3f(raw.x() * scale, raw.y() * scale, raw.z() * scale);
			cur_pos[i] = pm;
			cur_q[i] = q;
			bones[i].position = gse::vec3<gse::position>(gse::meters(pm.x()), gse::meters(pm.y()), gse::meters(pm.z()));
			bones[i].orientation = q;
			if (have_prev) {
				bones[i].velocity = gse::vec3<gse::velocity>(
					gse::meters_per_second((pm.x() - prev_pos[i].x()) / dt_s),
					gse::meters_per_second((pm.y() - prev_pos[i].y()) / dt_s),
					gse::meters_per_second((pm.z() - prev_pos[i].z()) / dt_s));
				bones[i].angular_velocity = gse::difference_axis_angle(prev_q[i], q) / dt;
			}
		}

		auto kin = reference_kinematics{};
		kin.pelvis_position = bones[0].position;
		kin.pelvis_orientation = bones[0].orientation;
		kin.pelvis_velocity = bones[0].velocity;
		kin.hip_angle_l = import_hinge_x(bones[0].orientation, bones[9].orientation);
		kin.knee_angle_l = import_hinge_x(bones[9].orientation, bones[10].orientation);
		kin.ankle_angle_l = import_hinge_x(bones[10].orientation, bones[11].orientation);
		kin.hip_angle_r = import_hinge_x(bones[0].orientation, bones[12].orientation);
		kin.knee_angle_r = import_hinge_x(bones[12].orientation, bones[13].orientation);
		kin.ankle_angle_r = import_hinge_x(bones[13].orientation, bones[14].orientation);
		kin.phi = static_cast<float>(frames_written % 60) / 60.f;
		kin.foot_grounded_l = bones[11].position.y() < gse::meters(0.06f);
		kin.foot_grounded_r = bones[14].position.y() < gse::meters(0.06f);

		auto obs = observation{};
		obs.pelvis_height = bones[0].position.y() - std::min(bones[11].position.y(), bones[14].position.y());
		obs.velocity_body = gse::inverse_rotate_vector(kin.pelvis_orientation, bones[0].velocity);
		obs.hip_angle_l = kin.hip_angle_l;
		obs.knee_angle_l = kin.knee_angle_l;
		obs.ankle_angle_l = kin.ankle_angle_l;
		obs.hip_angle_r = kin.hip_angle_r;
		obs.knee_angle_r = kin.knee_angle_r;
		obs.ankle_angle_r = kin.ankle_angle_r;
		if (have_prev) {
			obs.hip_rate_l = (kin.hip_angle_l - prev_angles[0]) / dt;
			obs.knee_rate_l = (kin.knee_angle_l - prev_angles[1]) / dt;
			obs.ankle_rate_l = (kin.ankle_angle_l - prev_angles[2]) / dt;
			obs.hip_rate_r = (kin.hip_angle_r - prev_angles[3]) / dt;
			obs.knee_rate_r = (kin.knee_angle_r - prev_angles[4]) / dt;
			obs.ankle_rate_r = (kin.ankle_angle_r - prev_angles[5]) / dt;
		}
		prev_angles = { kin.hip_angle_l, kin.knee_angle_l, kin.ankle_angle_l, kin.hip_angle_r, kin.knee_angle_r, kin.ankle_angle_r };
		const auto act = action{};
		const auto diag = actuation_diagnostics{};
		write_pod(out, obs);
		write_pod(out, act);
		write_pod(out, diag);
		write_pod(out, kin);
		for (const auto& b : bones) {
			write_pod(out, b);
		}

		prev_pos = cur_pos;
		prev_q = cur_q;
		have_prev = true;
		++frames_written;
	}

	gse::log::println("import_mocap: wrote {} frames (stride={}, scale={:.5f}) -> '{}'", frames_written, stride, scale, out_path);
	return static_cast<bool>(out);
}

auto gs::locomotion::clip_kinematics_stats(const std::string_view path) -> bool {
	const auto clip = load_reference_clip(path);
	if (!clip) {
		gse::log::println("clip_stats: FAILED to load '{}'", path);
		return false;
	}
	struct acc {
		float mn = 1e30f;
		float mx = -1e30f;
		double sum = 0.0;
		int n = 0;
		auto add(const float v) -> void {
			mn = std::min(mn, v);
			mx = std::max(mx, v);
			sum += static_cast<double>(v);
			++n;
		}
		auto mean() const -> float {
			return n > 0 ? static_cast<float>(sum / static_cast<double>(n)) : 0.f;
		}
	};
	auto hip_l = acc{}, hip_r = acc{}, knee_l = acc{}, knee_r = acc{}, ankle_l = acc{}, ankle_r = acc{};
	auto knee_rate = acc{}, pelvis_h = acc{}, foot_sep = acc{}, foot_fwd = acc{};
	const auto to_rad = [](const gse::angle a) { return a / gse::radians(1.f); };
	for (const auto& fr : clip->frames) {
		hip_l.add(to_rad(fr.kin.hip_angle_l));
		hip_r.add(to_rad(fr.kin.hip_angle_r));
		knee_l.add(to_rad(fr.kin.knee_angle_l));
		knee_r.add(to_rad(fr.kin.knee_angle_r));
		ankle_l.add(to_rad(fr.kin.ankle_angle_l));
		ankle_r.add(to_rad(fr.kin.ankle_angle_r));
		knee_rate.add(fr.obs.knee_rate_l / gse::radians_per_second(1.f));
		pelvis_h.add(fr.obs.pelvis_height / gse::meters(1.f));
		if (fr.bones.size() >= 17) {
			const auto pelvis = fr.bones[0].position;
			const auto fl = fr.bones[11].position;
			const auto fr_r = fr.bones[14].position;
			foot_sep.add(gse::abs(fl.x() - fr_r.x()) / gse::meters(1.f));
			foot_fwd.add((fl.z() - pelvis.z()) / gse::meters(1.f));
		}
	}
	const auto line = [](const std::string_view name, const acc& a) {
		gse::log::println("clip_stats:   {:14} min={:+.3f} max={:+.3f} mean={:+.3f}", name, a.mn, a.mx, a.mean());
	};
	gse::log::println("clip_stats: '{}' frames={}", path, clip->frames.size());
	line("hip_l rad", hip_l);
	line("hip_r rad", hip_r);
	line("knee_l rad", knee_l);
	line("knee_r rad", knee_r);
	line("ankle_l rad", ankle_l);
	line("ankle_r rad", ankle_r);
	line("knee_rate_l", knee_rate);
	line("pelvis_h m", pelvis_h);
	line("foot_sep m", foot_sep);
	line("foot_fwd m", foot_fwd);
	return true;
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
