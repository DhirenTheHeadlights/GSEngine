export module gs:locomotion_trainer;

import std;
import gse;

import :locomotion_types;
import :locomotion_mdp;
import :locomotion_nn;
import :locomotion_recorder;

export namespace gs::locomotion {
	struct ppo_config {
		std::size_t obs_dim = 30;
		std::size_t act_dim = 10;
		std::size_t hidden_dim = 128;
		std::size_t n_envs = 32;
		std::size_t rollout_steps = 1024;
		std::size_t mini_batch = 256;
		int n_epochs = 4;
		float lr = 3e-4f;
		float gamma = 0.99f;
		float lam = 0.95f;
		float clip_eps = 0.2f;
		float value_coeff = 0.5f;
		float entropy_coeff = 0.004f;
		int max_steps = 400;
		std::string checkpoint_path = "locomotion_checkpoint.bin";
		std::string state_path = "locomotion_train_state.bin";
		int checkpoint_every = 50;
		unsigned seed = 1234;
		float r_alive = 0.5f;
		float r_upright = 1.0f;
		float r_height = 0.5f;
		float r_track = 1.5f;
		float r_heading = 0.3f;
		float r_energy = 0.05f;
		float r_smooth = 0.1f;
		float motor_assist_force = 0.f;
		int motor_assist_updates = 500;
		int obs_lag = 0;
		float action_scale = 1.0f;
		bool resume = false;
		float perturb_impulse = 0.0f;
		int perturb_interval = 45;
		int perturb_grace = 25;
		std::string reference_clip_path = "";
		float rsi_min_speed = 0.15f;
		float rsi_max_speed = 0.65f;
		float r_imitation = 6.0f;
		float imitation_pose_scale = 2.0f;
		float imitation_vel_scale = 0.1f;
		float imitation_ee_scale = 10.0f;
		float imitation_w_pose = 0.5f;
		float imitation_w_vel = 0.2f;
		float imitation_w_ee = 0.3f;
		float imitation_term_threshold = 0.1f;
		int imitation_term_grace = 5;
		float w_task = 1.0f;
		float w_style = 2.0f;
		float disc_lr = 2e-5f;
		float disc_r1_weight = 8.0f;
		std::size_t disc_batch = 256;
		int disc_updates = 1;
		float disc_target_loss = 0.45f;
	};

	struct rollout_buffer {
		std::vector<float> obs;
		std::vector<float> actions;
		std::vector<float> log_probs;
		std::vector<float> values;
		std::vector<float> rewards;
		std::vector<float> dones;
		std::vector<float> advantages;
		std::vector<float> returns;
		std::vector<std::uint32_t> env_of;
		std::vector<float> amp_transitions;
		std::size_t capacity = 0;
		std::size_t size = 0;
		std::size_t obs_dim = 0;
		std::size_t act_dim = 0;

		rollout_buffer() = default;
		explicit rollout_buffer(
			std::size_t cap,
			std::size_t od,
			std::size_t ad
		);
		auto full() const -> bool;
		auto clear() -> void;
	};

	enum class train_stage : std::uint8_t {
		locating_scene,
		running
	};

	struct body_pose {
		gse::id id;
		gse::vec3<gse::position> position;
		gse::quat orientation;
	};

	struct env_state {
		gse::id owner_id = {};
		std::size_t env_index = 0;
		float bootstrap = 0.0f;
		gse::velocity cmd_forward = gse::meters_per_second(0.f);
		gse::velocity cmd_strafe = gse::meters_per_second(0.f);
		bool drives_initialized = false;
		bool snapshot_taken = false;
		bool has_prev = false;
		int reset_gate = 0;
		int episode = 0;
		int episode_steps = 0;
		int perturb_timer = 0;
		int recover_grace = 0;
		std::size_t ref_index = 0;
		bool rsi_active = false;
		float episode_reward = 0.0f;
		float imitation_accum = 0.0f;
		float track_err_accum = 0.0f;
		float prev_value = 0.0f;
		float prev_log_prob = 0.0f;
		std::vector<body_pose> initial_poses;
		std::vector<float> obs_buf;
		std::vector<float> prev_obs_buf;
		std::vector<float> action_buf;
		std::vector<float> prev_action_buf;
		std::vector<float> features;
		std::vector<float> prev_features;
		std::vector<float> mean_buf;
		std::vector<float> ah1_buf;
		std::vector<float> ah2_buf;
		std::vector<float> vh1_buf;
		std::vector<float> vh2_buf;
		float val_buf = 0.0f;
		std::deque<state> lag_ring;
	};
}

export namespace gs::locomotion::trainer {
	struct [[= gse::system_state<"Locomotion Trainer">{}]] data {
		std::mt19937 rng;

		actor_params actor;
		mlp critic;
		actor_adam actor_opt;
		critic_adam critic_opt;

		mlp discriminator;
		critic_adam disc_opt;
		amp_dataset dataset;
		bool dataset_built = false;

		rollout_buffer buffer;

		train_stage stage = train_stage::locating_scene;
		std::optional<gse::id> scene_id;
		int total_steps = 0;
		int update_count = 0;
		int episode = 0;
		float survival_sum = 0.0f;
		int survival_count = 0;
		float last_mean_surv = 0.0f;
		float best_mean_surv = 0.0f;

		std::vector<env_state> envs;

		reference_clip ref_clip;
		std::vector<std::size_t> rsi_frames;
		std::size_t cycle_start = 0;
		std::size_t cycle_end = 0;
		gse::velocity cycle_speed = gse::meters_per_second(0.f);
		bool cycle_loop = false;
		bool rsi_enabled = false;

		bool ready = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		const ppo_config& cfg,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<skeleton_refs> refs,
		gse::read<state> states,
		gse::write<gse::physics::joint_drive_component> drives,
		gse::write<gse::physics::transform_component> transforms,
		gse::write<gse::physics::motion_component> motions,
		gse::write<gse::physics::motor_component> motors
	) -> gse::async::task<>;
}

export namespace gs::locomotion::pose_player {
	struct owner_play_state {
		bool drives_init = false;
		bool snapshotted = false;
		float phi = 0.0f;
		std::size_t ref_index = 0;
		int steps_alive = 0;
		int reset_gate = 0;
		std::vector<body_pose> initial_poses;
		std::vector<float> obs_buf;
		std::vector<float> h1_buf;
		std::vector<float> h2_buf;
		std::vector<float> mean_buf;
	};

	struct [[= gse::system_state<"Pose Player">{}]] data {
		actor_params actor;
		mlp critic;
		bool attempted = false;
		bool loaded = false;
		bool logged_owner_count = false;
		std::optional<gse::id> scene_id;
		std::unordered_map<std::uint64_t, owner_play_state> owners;
		reference_clip ref_clip;
		std::vector<std::size_t> rsi_frames;
		std::size_t cycle_start = 0;
		std::size_t cycle_end = 0;
		bool cycle_loop = false;
		float cmd_forward = 0.3f;
		std::mt19937 rng;
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		const ppo_config& cfg,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<skeleton_refs> refs,
		gse::read<state> states,
		gse::write<gse::physics::joint_drive_component> drives,
		gse::write<gse::physics::transform_component> transforms,
		gse::write<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

namespace gs::locomotion {
	auto compute_gae(
		rollout_buffer& buf,
		std::span<const float> env_bootstrap,
		float gamma,
		float lam
	) -> void;

	auto run_ppo_update(
		trainer::data& d,
		const ppo_config& cfg
	) -> void;

	auto update_amp_rewards(
		trainer::data& d,
		const ppo_config& cfg
	) -> disc_metrics;

	auto initialize_drives(
		const skeleton_refs& r,
		gse::write<gse::physics::joint_drive_component>& drives
	) -> void;

	auto compute_reward(
		const state& s,
		gse::velocity cmd_forward,
		gse::velocity cmd_strafe,
		gse::position target_height,
		const ppo_config& cfg
	) -> float;

	auto compute_imitation_reward(
		const state& s,
		const reference_frame& ref,
		const skeleton_refs& r,
		const ppo_config& cfg
	) -> float;

	auto sample_command(
		env_state& env,
		std::mt19937& rng,
		bool aligned,
		gse::velocity aligned_speed
	) -> void;

	auto episode_done(
		const state& s,
		int steps,
		int max_steps
	) -> bool;

	auto make_env_state(
		gse::id owner_id,
		std::size_t env_index,
		const ppo_config& cfg
	) -> env_state;

	auto take_env_snapshot(
		env_state& env,
		const skeleton_refs& r,
		gse::write<gse::physics::transform_component>& transforms
	) -> void;

	auto reset_to_reference(
		std::span<const body_pose> initial_poses,
		const skeleton_refs& r,
		const reference_frame& frame,
		gse::write<gse::physics::transform_component>& transforms,
		gse::write<gse::physics::motion_component>& motions
	) -> void;

	struct walk_extract {
		std::vector<std::size_t> frames;
		std::size_t cycle_start = 0;
		std::size_t cycle_end = 0;
		gse::velocity cycle_speed = gse::meters_per_second(0.f);
		bool cycle_loop = false;
	};

	auto extract_walk_frames(
		const reference_clip& clip,
		float min_speed,
		float max_speed
	) -> walk_extract;

	auto pack_reference_pose(
		const reference_frame& ref,
		std::span<float> buf
	) -> void;

	auto reset_env(
		env_state& env,
		const skeleton_refs& r,
		gse::write<gse::physics::transform_component>& transforms,
		gse::write<gse::physics::motion_component>& motions,
		gse::write<gse::physics::joint_drive_component>& drives,
		const reference_frame* rsi
	) -> void;
}

gs::locomotion::rollout_buffer::rollout_buffer(const std::size_t cap, const std::size_t od, const std::size_t ad)
	: obs(cap * od, 0.0f)
	, actions(cap * ad, 0.0f)
	, log_probs(cap, 0.0f)
	, values(cap, 0.0f)
	, rewards(cap, 0.0f)
	, dones(cap, 0.0f)
	, advantages(cap, 0.0f)
	, returns(cap, 0.0f)
	, env_of(cap, 0u)
	, amp_transitions(cap * 2 * amp_feature_dim, 0.0f)
	, capacity(cap)
	, obs_dim(od)
	, act_dim(ad) {}

auto gs::locomotion::rollout_buffer::full() const -> bool {
	return size >= capacity;
}

auto gs::locomotion::rollout_buffer::clear() -> void {
	size = 0;
}

auto gs::locomotion::compute_gae(rollout_buffer& buf, const std::span<const float> env_bootstrap, const float gamma, const float lam) -> void {
	for (std::size_t e = 0; e < env_bootstrap.size(); ++e) {
		auto idxs = std::vector<std::size_t>{};
		for (std::size_t t = 0; t < buf.size; ++t) {
			if (buf.env_of[t] == e) {
				idxs.push_back(t);
			}
		}
		auto last_gae = 0.0f;
		for (int k = static_cast<int>(idxs.size()) - 1; k >= 0; --k) {
			const auto t = idxs[static_cast<std::size_t>(k)];
			const auto mask = 1.0f - buf.dones[t];
			const auto next = static_cast<std::size_t>(k) + 1;
			const auto v_next = next < idxs.size() ? buf.values[idxs[next]] : env_bootstrap[e];
			const auto delta = buf.rewards[t] + gamma * v_next * mask - buf.values[t];
			last_gae = delta + gamma * lam * mask * last_gae;
			buf.advantages[t] = last_gae;
			buf.returns[t] = last_gae + buf.values[t];
		}
	}
}

auto gs::locomotion::run_ppo_update(trainer::data& d, const ppo_config& cfg) -> void {
	const auto n = d.buffer.size;
	const auto obs_dim = cfg.obs_dim;
	const auto act_dim = cfg.act_dim;

	auto mean_adv = 0.0f;
	for (std::size_t t = 0; t < n; ++t) {
		mean_adv += d.buffer.advantages[t];
	}
	mean_adv /= static_cast<float>(n);
	auto var_adv = 0.0f;
	for (std::size_t t = 0; t < n; ++t) {
		const auto diff = d.buffer.advantages[t] - mean_adv;
		var_adv += diff * diff;
	}
	var_adv /= static_cast<float>(n);
	const auto std_adv = std::sqrt(var_adv + 1e-8f);
	for (auto& adv : d.buffer.advantages) {
		adv = (adv - mean_adv) / std_adv;
	}

	const auto obs_all = std::mdspan(d.buffer.obs.data(), n, obs_dim);
	const auto acts_all = std::mdspan(d.buffer.actions.data(), n, act_dim);

	auto actor_loss = 0.0f;
	auto critic_loss = 0.0f;
	int batches = 0;

	for (int epoch = 0; epoch < cfg.n_epochs; ++epoch) {
		for (std::size_t start = 0; start < n; start += cfg.mini_batch) {
			const auto end = std::min(start + cfg.mini_batch, n);
			const auto n_batch = end - start;

			actor_loss += ppo_actor_update(
				d.actor,
				d.actor_opt,
				std::submdspan(obs_all, std::pair{start, end}, std::full_extent),
				std::submdspan(acts_all, std::pair{start, end}, std::full_extent),
				std::span(d.buffer.log_probs).subspan(start, n_batch),
				std::span(d.buffer.advantages).subspan(start, n_batch),
				cfg.lr,
				cfg.clip_eps,
				cfg.entropy_coeff
			);
			critic_loss += ppo_critic_update(
				d.critic,
				d.critic_opt,
				std::submdspan(obs_all, std::pair{start, end}, std::full_extent),
				std::span(d.buffer.returns).subspan(start, n_batch),
				cfg.lr,
				cfg.value_coeff
			);
			++batches;
		}
	}

	++d.update_count;

	gse::log::println(
		"locomotion_train: update={} steps={} actor_loss={:.4f} critic_loss={:.4f}",
		d.update_count,
		d.total_steps,
		actor_loss / static_cast<float>(batches),
		critic_loss / static_cast<float>(batches)
	);

	if (d.last_mean_surv > d.best_mean_surv) {
		d.best_mean_surv = d.last_mean_surv;
		checkpoint_save(d.actor, d.critic, cfg.checkpoint_path);
		gse::log::println("locomotion_train: new best mean_surv={:.1f} saved to '{}'", d.best_mean_surv, cfg.checkpoint_path);
	}
	if (d.update_count % cfg.checkpoint_every == 0) {
		const auto meta = train_meta{
			.update_count = d.update_count,
			.total_steps = d.total_steps,
			.best_mean_surv = d.best_mean_surv,
		};
		checkpoint_save_full(d.actor, d.actor_opt, d.critic, d.critic_opt, d.discriminator, d.disc_opt, meta, cfg.state_path);
	}
}

auto gs::locomotion::update_amp_rewards(trainer::data& d, const ppo_config& cfg) -> disc_metrics {
	constexpr auto pair_dim = 2 * amp_feature_dim;
	auto metrics = disc_metrics{};
	if (!d.dataset_built || d.dataset.transition_starts.empty() || d.buffer.size == 0) {
		return metrics;
	}
	const auto batch = std::min(cfg.disc_batch, d.buffer.size);
	auto real_buf = std::vector<float>(batch * pair_dim);
	auto fake_buf = std::vector<float>(batch * pair_dim);
	auto pair = std::array<float, pair_dim>{};
	auto fake_pick = std::uniform_int_distribution<std::size_t>(0, d.buffer.size - 1);
	for (auto u = 0; u < cfg.disc_updates; ++u) {
		for (std::size_t b = 0; b < batch; ++b) {
			sample_amp_transition(d.dataset, d.rng, pair);
			std::ranges::copy(pair, real_buf.begin() + static_cast<std::ptrdiff_t>(b * pair_dim));
			const auto* src = d.buffer.amp_transitions.data() + fake_pick(d.rng) * pair_dim;
			std::copy(src, src + pair_dim, fake_buf.begin() + static_cast<std::ptrdiff_t>(b * pair_dim));
		}
		const auto real = std::mdspan<const float, std::dextents<std::size_t, 2>>(real_buf.data(), batch, pair_dim);
		const auto fake = std::mdspan<const float, std::dextents<std::size_t, 2>>(fake_buf.data(), batch, pair_dim);
		metrics = discriminator_update(d.discriminator, d.disc_opt, real, fake, cfg.disc_lr, cfg.disc_r1_weight, cfg.disc_target_loss);
	}
	auto style_sum = 0.0f;
	for (std::size_t t = 0; t < d.buffer.size; ++t) {
		const auto trans = std::span<const float>(d.buffer.amp_transitions).subspan(t * pair_dim, pair_dim);
		const auto r_style = discriminator_style_reward(d.discriminator, trans);
		style_sum += r_style;
		d.buffer.rewards[t] = cfg.w_task * d.buffer.rewards[t] + cfg.w_style * r_style;
	}
	const auto mean_style = style_sum / static_cast<float>(d.buffer.size);
	const auto mean_surv = d.survival_count > 0 ? d.survival_sum / static_cast<float>(d.survival_count) : 0.0f;
	d.last_mean_surv = mean_surv;
	d.survival_sum = 0.0f;
	d.survival_count = 0;
	gse::log::println("locomotion_train: amp total_steps={} mean_surv={:.1f} D_loss={:.3f} D(real)={:.3f} D(fake)={:.3f} r1={:.4f} mean_style={:.3f}", d.total_steps, mean_surv, metrics.loss, metrics.mean_real, metrics.mean_fake, metrics.r1, mean_style);
	return metrics;
}

auto gs::locomotion::initialize_drives(const skeleton_refs& r, gse::write<gse::physics::joint_drive_component>& drives) -> void {
	const auto hip_s = gse::vec3<gse::angular_stiffness>(
		gse::newton_meters_per_radian(600.f),
		gse::newton_meters_per_radian(250.f),
		gse::newton_meters_per_radian(250.f)
	);
	const auto knee_s = gse::vec3<gse::angular_stiffness>(
		gse::newton_meters_per_radian(650.f),
		gse::newton_meters_per_radian(0.f),
		gse::newton_meters_per_radian(0.f)
	);
	const auto ankle_s = gse::vec3<gse::angular_stiffness>(
		gse::newton_meters_per_radian(400.f),
		gse::newton_meters_per_radian(0.f),
		gse::newton_meters_per_radian(0.f)
	);

	const auto set_drive = [&](const gse::id joint_id, const gse::vec3<gse::angular_stiffness> stiff, const gse::torque max_t) {
		if (auto* drv = drives.find(joint_id)) {
			drv->enabled = true;
			drv->stiffness = stiff;
			drv->damping = 6.0f;
			drv->max_torque = max_t;
		}
	};

	set_drive(r.hip_l_joint_id, hip_s, gse::newton_meters(380.f));
	set_drive(r.knee_l_joint_id, knee_s, gse::newton_meters(420.f));
	set_drive(r.ankle_l_joint_id, ankle_s, gse::newton_meters(160.f));
	set_drive(r.hip_r_joint_id, hip_s, gse::newton_meters(380.f));
	set_drive(r.knee_r_joint_id, knee_s, gse::newton_meters(420.f));
	set_drive(r.ankle_r_joint_id, ankle_s, gse::newton_meters(160.f));
}

auto gs::locomotion::compute_reward(const state& s, const gse::velocity cmd_forward, const gse::velocity cmd_strafe, const gse::position target_height, const ppo_config& cfg) -> float {
	const auto ref_speed = gse::meters_per_second(1.0f);
	const auto heading_limit = gse::radians(0.6f);

	const auto forward_speed = -s.velocity_body.z();
	const auto right_speed = s.velocity_body.x();
	const auto err_forward = (forward_speed - cmd_forward) / ref_speed;
	const auto err_right = (right_speed - cmd_strafe) / ref_speed;
	const auto track = std::exp(-4.0f * (err_forward * err_forward + err_right * err_right));

	const auto up = cross(s.pelvis_right, s.pelvis_forward);
	const auto upright = std::clamp(up.y(), 0.0f, 1.0f);

	const auto pelvis_height = s.pelvis_position.y() - s.support_center.y();
	const auto target_above_support = target_height - s.support_center.y();
	const auto height = std::clamp(pelvis_height / target_above_support, 0.0f, 1.0f);

	const auto current_yaw = std::atan2(-s.pelvis_forward.x(), -s.pelvis_forward.z());
	const auto heading_ratio = gse::radians(current_yaw) / heading_limit;
	const auto heading = std::exp(-heading_ratio * heading_ratio);

	const auto stability = upright * height;

	return cfg.r_alive
		+ cfg.r_upright * upright
		+ cfg.r_height * height
		+ stability * (cfg.r_track * track + cfg.r_heading * heading);
}

auto gs::locomotion::compute_imitation_reward(const state& s, const reference_frame& ref, const skeleton_refs& r, const ppo_config& cfg) -> float {
	const auto angle_err = [](const gse::angle a, const gse::angle b) {
		const auto d = (a - b) / gse::radians(1.0f);
		return d * d;
	};
	const auto rate_err = [](const gse::angular_velocity a, const gse::angular_velocity b) {
		const auto d = (a - b) / gse::radians_per_second(1.0f);
		return d * d;
	};

	const auto pose_err =
		angle_err(s.hip_angle_l, ref.kin.hip_angle_l) +
		angle_err(s.knee_angle_l, ref.kin.knee_angle_l) +
		angle_err(s.ankle_angle_l, ref.kin.ankle_angle_l) +
		angle_err(s.hip_angle_r, ref.kin.hip_angle_r) +
		angle_err(s.knee_angle_r, ref.kin.knee_angle_r) +
		angle_err(s.ankle_angle_r, ref.kin.ankle_angle_r);

	const auto vel_err =
		rate_err(s.hip_rate_l, ref.obs.hip_rate_l) +
		rate_err(s.knee_rate_l, ref.obs.knee_rate_l) +
		rate_err(s.ankle_rate_l, ref.obs.ankle_rate_l) +
		rate_err(s.hip_rate_r, ref.obs.hip_rate_r) +
		rate_err(s.knee_rate_r, ref.obs.knee_rate_r) +
		rate_err(s.ankle_rate_r, ref.obs.ankle_rate_r);

	const auto bone_index = [&r](const gse::id id) -> std::size_t {
		for (std::size_t i = 0; i < r.all_bone_ids.size(); ++i) {
			if (r.all_bone_ids[i] == id) {
				return i;
			}
		}
		return 0;
	};
	const auto foot_local = [](const gse::vec3<gse::position>& foot, const gse::vec3<gse::position>& pelvis, const gse::vec3f& fwd, const gse::vec3f& right) {
		const auto rel = foot - pelvis;
		return gse::vec3f(gse::dot(rel, right) / gse::meters(1.0f), rel.y() / gse::meters(1.0f), gse::dot(rel, fwd) / gse::meters(1.0f));
	};
	const auto sq = [](const gse::vec3f& v) {
		return v.x() * v.x() + v.y() * v.y() + v.z() * v.z();
	};
	auto ee_err = 0.0f;
	const auto fl = bone_index(r.foot_l_id);
	const auto fr = bone_index(r.foot_r_id);
	if (fl < ref.bones.size() && fr < ref.bones.size()) {
		const auto ref_pelvis = ref.bones[0].position;
		const auto ref_fwd = gse::rotate_vector(ref.kin.pelvis_orientation, gse::vec3f(0.f, 0.f, -1.f));
		const auto ref_right = gse::rotate_vector(ref.kin.pelvis_orientation, gse::vec3f(1.f, 0.f, 0.f));
		ee_err =
			sq(foot_local(s.foot_position_l, s.pelvis_position, s.pelvis_forward, s.pelvis_right) - foot_local(ref.bones[fl].position, ref_pelvis, ref_fwd, ref_right)) +
			sq(foot_local(s.foot_position_r, s.pelvis_position, s.pelvis_forward, s.pelvis_right) - foot_local(ref.bones[fr].position, ref_pelvis, ref_fwd, ref_right));
	}

	const auto r_pose = std::exp(-cfg.imitation_pose_scale * pose_err);
	const auto r_vel = std::exp(-cfg.imitation_vel_scale * vel_err);
	const auto r_ee = std::exp(-cfg.imitation_ee_scale * ee_err);
	return cfg.imitation_w_pose * r_pose + cfg.imitation_w_vel * r_vel + cfg.imitation_w_ee * r_ee;
}

auto gs::locomotion::pack_reference_pose(const reference_frame& ref, const std::span<float> buf) -> void {
	const gse::angle angles[6] = {
		ref.kin.hip_angle_l, ref.kin.knee_angle_l, ref.kin.ankle_angle_l,
		ref.kin.hip_angle_r, ref.kin.knee_angle_r, ref.kin.ankle_angle_r,
	};
	const gse::angular_velocity rates[6] = {
		ref.obs.hip_rate_l, ref.obs.knee_rate_l, ref.obs.ankle_rate_l,
		ref.obs.hip_rate_r, ref.obs.knee_rate_r, ref.obs.ankle_rate_r,
	};
	static_assert(sizeof(angles) == 6 * sizeof(float) && sizeof(rates) == 6 * sizeof(float));
	std::memcpy(buf.data(), angles, sizeof(angles));
	std::memcpy(buf.data() + 6, rates, sizeof(rates));
}

auto gs::locomotion::episode_done(const state& s, const int steps, const int max_steps) -> bool {
	const gse::position fall_height = gse::meters(0.4f);
	return s.pelvis_position.y() < fall_height || steps >= max_steps;
}

auto gs::locomotion::sample_command(env_state& env, std::mt19937& rng, const bool aligned, const gse::velocity aligned_speed) -> void {
	if (aligned) {
		env.cmd_forward = aligned_speed;
		env.cmd_strafe = gse::meters_per_second(0.f);
		return;
	}
	auto forward_dist = std::uniform_real_distribution<float>(0.25f, 0.55f);
	auto strafe_dist = std::uniform_real_distribution<float>(-0.1f, 0.1f);
	env.cmd_forward = gse::meters_per_second(forward_dist(rng));
	env.cmd_strafe = gse::meters_per_second(strafe_dist(rng));
}

auto gs::locomotion::make_env_state(const gse::id owner_id, const std::size_t env_index, const ppo_config& cfg) -> env_state {
	return {
		.owner_id = owner_id,
		.env_index = env_index,
		.obs_buf = std::vector<float>(cfg.obs_dim, 0.0f),
		.prev_obs_buf = std::vector<float>(cfg.obs_dim, 0.0f),
		.action_buf = std::vector<float>(cfg.act_dim, 0.0f),
		.prev_action_buf = std::vector<float>(cfg.act_dim, 0.0f),
		.features = std::vector<float>(amp_feature_dim, 0.0f),
		.prev_features = std::vector<float>(amp_feature_dim, 0.0f),
		.mean_buf = std::vector<float>(cfg.act_dim, 0.0f),
		.ah1_buf = std::vector<float>(cfg.hidden_dim, 0.0f),
		.ah2_buf = std::vector<float>(cfg.hidden_dim, 0.0f),
		.vh1_buf = std::vector<float>(cfg.hidden_dim, 0.0f),
		.vh2_buf = std::vector<float>(cfg.hidden_dim, 0.0f),
	};
}

auto gs::locomotion::take_env_snapshot(env_state& env, const skeleton_refs& r, gse::write<gse::physics::transform_component>& transforms) -> void {
	env.initial_poses.clear();
	for (const auto bone_id : r.all_bone_ids) {
		if (const auto* tc = transforms.find(bone_id)) {
			env.initial_poses.push_back({ bone_id, tc->position, tc->orientation });
		}
	}
	env.snapshot_taken = true;
}

auto gs::locomotion::reset_to_reference(const std::span<const body_pose> initial_poses, const skeleton_refs& r, const reference_frame& frame, gse::write<gse::physics::transform_component>& transforms, gse::write<gse::physics::motion_component>& motions) -> void {
	const auto bone_count = std::min(r.all_bone_ids.size(), frame.bones.size());
	if (initial_poses.empty() || bone_count == 0) {
		return;
	}
	const auto ref_pelvis = frame.bones[0].position;
	const auto ref_forward = gse::rotate_vector(frame.kin.pelvis_orientation, gse::vec3f(0.f, 0.f, -1.f));
	const auto ref_yaw = gse::atan2(-ref_forward.x(), -ref_forward.z());
	const auto align = gse::quat(gse::vec3f(0.f, 1.f, 0.f), -ref_yaw);
	const auto spawn_pelvis = initial_poses[0].position;
	const auto target_pelvis = gse::vec3<gse::position>(spawn_pelvis.x(), ref_pelvis.y(), spawn_pelvis.z());
	for (std::size_t i = 0; i < bone_count; ++i) {
		const auto bone_id = r.all_bone_ids[i];
		const auto& bs = frame.bones[i];
		const auto local = bs.position - ref_pelvis;
		if (auto* tc = transforms.find(bone_id)) {
			tc->position = target_pelvis + gse::rotate_vector(align, local);
			tc->orientation = align * bs.orientation;
		}
		if (auto* mc = motions.find(bone_id)) {
			mc->current_velocity = gse::rotate_vector(align, bs.velocity);
			mc->angular_velocity = gse::rotate_vector(align, bs.angular_velocity);
			mc->reset_pending = 1;
		}
	}
}

auto gs::locomotion::extract_walk_frames(const reference_clip& clip, const float min_speed, const float max_speed) -> walk_extract {
	auto result = walk_extract{};
	const auto& frames = clip.frames;
	if (frames.empty()) {
		return result;
	}
	const auto is_walk = [&](const std::size_t i) {
		const auto fwd = -frames[i].obs.velocity_body.z() / gse::meters_per_second(1.0f);
		return fwd >= min_speed && fwd <= max_speed;
	};
	const auto continuous = [&](const std::size_t i) {
		return gse::magnitude(frames[i].kin.pelvis_position - frames[i - 1].kin.pelvis_position) < gse::meters(0.5f);
	};
	auto wraps = std::vector<std::size_t>{};
	for (std::size_t i = 1; i < frames.size(); ++i) {
		if (frames[i].kin.phi < frames[i - 1].kin.phi - 0.5f) {
			wraps.push_back(i);
		}
	}
	for (std::size_t k = 0; k + 1 < wraps.size(); ++k) {
		const auto cs = wraps[k];
		const auto ce = wraps[k + 1] - 1;
		if (ce < cs + 15) {
			continue;
		}
		auto cont = true;
		auto speed_sum = gse::meters_per_second(0.f);
		for (std::size_t i = cs; i <= ce; ++i) {
			if (i > cs && !continuous(i)) {
				cont = false;
				break;
			}
			speed_sum += -frames[i].obs.velocity_body.z();
		}
		if (!cont) {
			continue;
		}
		const auto mean_vel = speed_sum / static_cast<float>(ce - cs + 1);
		const auto mean_speed = mean_vel / gse::meters_per_second(1.0f);
		if (mean_speed >= min_speed && mean_speed <= max_speed && (!result.cycle_loop || mean_vel > result.cycle_speed)) {
			result.cycle_start = cs;
			result.cycle_end = ce;
			result.cycle_speed = mean_vel;
			result.cycle_loop = true;
		}
	}
	if (result.cycle_loop) {
		for (std::size_t i = result.cycle_start; i <= result.cycle_end; ++i) {
			result.frames.push_back(i);
		}
	}
	else {
		for (std::size_t i = 0; i < frames.size(); ++i) {
			if (is_walk(i)) {
				result.frames.push_back(i);
			}
		}
	}
	return result;
}

auto gs::locomotion::reset_env(env_state& env, const skeleton_refs& r, gse::write<gse::physics::transform_component>& transforms, gse::write<gse::physics::motion_component>& motions, gse::write<gse::physics::joint_drive_component>& drives, const reference_frame* rsi) -> void {
	if (rsi) {
		reset_to_reference(env.initial_poses, r, *rsi, transforms, motions);
	}
	else {
		for (const auto& pose : env.initial_poses) {
			if (auto* tc = transforms.find(pose.id)) {
				tc->position = pose.position;
				tc->orientation = pose.orientation;
			}
			if (auto* mc = motions.find(pose.id)) {
				mc->current_velocity = {};
				mc->angular_velocity = {};
				mc->reset_pending = 1;
			}
		}
	}
	apply_action(action{}, r, drives);
	std::ranges::fill(env.action_buf, 0.0f);
	std::ranges::fill(env.prev_action_buf, 0.0f);
	env.lag_ring.clear();
	env.drives_initialized = false;
	env.has_prev = false;
	env.reset_gate = 30;
	env.episode_steps = 0;
	env.episode_reward = 0.0f;
	env.imitation_accum = 0.0f;
	env.track_err_accum = 0.0f;

	if (rsi && env.env_index == 0 && env.episode < 3) {
		const auto* tc = transforms.find(r.pelvis_id);
		const auto* mc = motions.find(r.pelvis_id);
		if (tc && mc) {
			gse::log::println("locomotion_train: RSI env0 ep={} phi={:.3f} pelvis={:.3f} vel={:.3f}", env.episode, rsi->kin.phi, tc->position, mc->current_velocity);
		}
	}
}

auto gs::locomotion::trainer::run(gse::context& ctx, data& d, const ppo_config& cfg, const gse::shared_view<gse::world_system::data> world_d, gse::read<skeleton_refs> refs, gse::read<state> states, gse::write<gse::physics::joint_drive_component> drives, gse::write<gse::physics::transform_component> transforms, gse::write<gse::physics::motion_component> motions, gse::write<gse::physics::motor_component> motors) -> gse::async::task<> {
	if (!d.ready) {
		gse::watchdog::stop();
		gse::log::println("locomotion_train: watchdog disabled (the PPO+discriminator update tick exceeds the 500ms scheduler-wait budget by design)");
		gse::log::println("locomotion_train: AMP config w_task={:.2f} w_style={:.2f} disc_lr={:.2e} disc_r1={:.1f} disc_batch={} disc_updates={} lr={:.2e} entropy={:.4f}", cfg.w_task, cfg.w_style, cfg.disc_lr, cfg.disc_r1_weight, cfg.disc_batch, cfg.disc_updates, cfg.lr, cfg.entropy_coeff);
		d.rng.seed(cfg.seed);
		d.actor = actor_make(cfg.obs_dim, cfg.act_dim, cfg.hidden_dim, d.rng);
		d.critic = mlp_make(cfg.obs_dim, cfg.hidden_dim, 1, 1.0f, d.rng);
		d.actor_opt = actor_adam_make(d.actor);
		d.critic_opt = critic_adam_make(d.critic);
		d.discriminator = mlp_make(2 * amp_feature_dim, cfg.hidden_dim, 1, 0.1f, d.rng);
		d.disc_opt = critic_adam_make(d.discriminator);
		d.buffer = rollout_buffer(cfg.rollout_steps, cfg.obs_dim, cfg.act_dim);

		if (cfg.resume) {
			auto meta = train_meta{};
			if (checkpoint_load_full(d.actor, d.actor_opt, d.critic, d.critic_opt, d.discriminator, d.disc_opt, meta, cfg.state_path)) {
				d.update_count = meta.update_count;
				d.total_steps = meta.total_steps;
				d.best_mean_surv = meta.best_mean_surv;
				d.rng.seed(cfg.seed + static_cast<unsigned>(d.update_count));
				gse::log::println("locomotion_train: RESUMED full state from '{}' (update={} steps={} best_surv={:.1f})", cfg.state_path, d.update_count, d.total_steps, d.best_mean_surv);
			}
			else if (checkpoint_load(d.actor, d.critic, cfg.checkpoint_path)) {
				gse::log::println("locomotion_train: RESUMED policy-only warm-start from '{}'", cfg.checkpoint_path);
			}
		}

		if (!cfg.reference_clip_path.empty()) {
			if (auto clip = load_reference_clip(cfg.reference_clip_path)) {
				d.ref_clip = std::move(*clip);
				const auto we = extract_walk_frames(d.ref_clip, cfg.rsi_min_speed, cfg.rsi_max_speed);
				d.rsi_frames = we.frames;
				d.cycle_start = we.cycle_start;
				d.cycle_end = we.cycle_end;
				d.cycle_speed = we.cycle_speed;
				d.cycle_loop = we.cycle_loop;
				d.rsi_enabled = !d.rsi_frames.empty();
				if (we.cycle_loop) {
					const auto& frames = d.ref_clip.frames;
					gse::log::println("locomotion_train: RSI clean walk cycle [{},{}] len={} speed={:.3f} phi=[{:.3f},{:.3f}] (looping)", d.cycle_start, d.cycle_end, d.cycle_end - d.cycle_start + 1, d.cycle_speed, frames[d.cycle_start].kin.phi, frames[d.cycle_end].kin.phi);
				}
				else {
					gse::log::println("locomotion_train: RSI no clean cycle — using {} scattered walk frames (no loop)", d.rsi_frames.size());
				}
			}
			else {
				gse::log::println("locomotion_train: RSI clip '{}' FAILED to load — using neutral-spawn reset", cfg.reference_clip_path);
			}
		}

		d.ready = true;
	}

	if (!d.scene_id.has_value()) {
		for (const auto& scene_id : world_d.scene_ids) {
			if (scene_id.tag() == std::string_view("Training")) {
				d.scene_id = scene_id;
				break;
			}
		}
	}

	if (d.stage == train_stage::locating_scene) {
		if (d.scene_id.has_value() && world_d.active_scene != d.scene_id) {
			ctx.channels.push<gse::activate_scene_request>({ .scene_id = *d.scene_id });
		}
		if (d.scene_id.has_value() && world_d.active_scene == d.scene_id && d.envs.empty()) {
			const auto owner_ids = refs.owner_ids();
			if (owner_ids.empty()) {
				return {};
			}
			d.envs.reserve(owner_ids.size());
			for (std::size_t i = 0; i < owner_ids.size(); ++i) {
				d.envs.push_back(make_env_state(owner_ids[i], i, cfg));
			}
			if (!d.dataset_built && !d.ref_clip.frames.empty()) {
				if (const auto* r0 = refs.find(owner_ids[0])) {
					d.dataset = build_amp_dataset(std::span(&d.ref_clip, 1), *r0);
					d.dataset_built = !d.dataset.transition_starts.empty();
					gse::log::println("locomotion_train: AMP dataset {} frames {} transitions", d.dataset.frame_count, d.dataset.transition_starts.size());
				}
			}
			d.stage = train_stage::running;
			gse::log::println("locomotion_train: {} envs discovered, begin training", d.envs.size());
		}
		return {};
	}

	const auto reset_episode = [&](env_state& e, const skeleton_refs& sr) {
		d.survival_sum += static_cast<float>(e.episode_steps);
		++d.survival_count;
		const reference_frame* rsi = nullptr;
		if (d.rsi_enabled && !d.rsi_frames.empty()) {
			auto pick = std::uniform_int_distribution<std::size_t>(0, d.rsi_frames.size() - 1);
			const auto fi = d.rsi_frames[pick(d.rng)];
			rsi = &d.ref_clip.frames[fi];
			e.ref_index = fi;
		}
		reset_env(e, sr, transforms, motions, drives, rsi);
		e.rsi_active = (rsi != nullptr);
		e.perturb_timer = cfg.perturb_interval;
		e.recover_grace = 0;
		sample_command(e, d.rng, d.cycle_loop, d.cycle_speed);
		++e.episode;
		++d.episode;
	};

	for (auto& env : d.envs) {
		const auto* r = refs.find(env.owner_id);
		const auto* s = states.find(env.owner_id);
		if (!r || !s || !s->valid) {
			continue;
		}

		const state* os_ptr = s;
		if (cfg.obs_lag > 0) {
			env.lag_ring.push_back(*s);
			while (env.lag_ring.size() > static_cast<std::size_t>(cfg.obs_lag) + 1) {
				env.lag_ring.pop_front();
			}
			os_ptr = &env.lag_ring.front();
		}
		const state& os = *os_ptr;

		if (!env.snapshot_taken) {
			take_env_snapshot(env, *r, transforms);
		}

		if (!env.drives_initialized) {
			initialize_drives(*r, drives);
			sample_command(env, d.rng, d.cycle_loop, d.cycle_speed);
			env.drives_initialized = true;
			continue;
		}

		if (env.reset_gate == 0 && (!gse::isfinite(os.pelvis_position) || !gse::isfinite(os.velocity_body))) {
			gse::log::println("locomotion_train: env={} non-finite state at step {}, force reset", env.env_index, env.episode_steps);
			reset_episode(env, *r);
			continue;
		}

		const auto cmd = intent{
			.forward = env.cmd_forward / gse::meters_per_second(1.0f),
			.strafe = env.cmd_strafe / gse::meters_per_second(1.0f),
			.desired_yaw = gse::radians(0.f),
			.has_heading = true,
		};
		pack_observation(observe(os, cmd, gait{}, 0.0f), env.obs_buf);
		auto pelvis_av = gse::vec3<gse::angular_velocity>{};
		if (const auto* mc = motions.find(r->pelvis_id)) {
			pelvis_av = mc->angular_velocity;
		}
		amp_features_state(os, pelvis_av, std::span(env.features).first<amp_feature_dim>());

		if (auto* mot = motors.find(env.owner_id)) {
			const auto denom = static_cast<float>(std::max(1, cfg.motor_assist_updates));
			const auto progress = std::min(1.0f, static_cast<float>(d.update_count) / denom);
			const auto forward = horizontal_axis(os.pelvis_forward);
			const auto right = horizontal_axis(os.pelvis_right);
			mot->velocity_drive_target = forward * env.cmd_forward + right * env.cmd_strafe;
			mot->max_force = env.reset_gate == 0 ? gse::newtons(cfg.motor_assist_force * (1.0f - progress)) : gse::newtons(0.f);
		}

		// The GPU solver is authoritative and the readback lags the solve by a few
		// ticks, so a per-env reset is not observable in `state` for several ticks
		// after reset_env runs. Hold the reference pose and suppress episode_done
		// until the obs confirms the reset landed (pelvis back near the reset
		// height), otherwise episode_done fires on the stale pre-reset state.
		if (env.reset_gate > 0) {
			apply_action(action{}, *r, drives);
			if (os.pelvis_position.y() >= gse::meters(0.9f)) {
				env.reset_gate = 0;
			}
			else {
				--env.reset_gate;
			}
			continue;
		}

		if (env.has_prev) {
			auto energy = 0.0f;
			auto smooth = 0.0f;
			for (std::size_t i = 0; i < cfg.act_dim; ++i) {
				const auto a = env.action_buf[i];
				const auto da = a - env.prev_action_buf[i];
				energy += a * a;
				smooth += da * da;
			}
			const auto inv_act = 1.0f / static_cast<float>(cfg.act_dim);
			const auto reward = compute_reward(os, env.cmd_forward, env.cmd_strafe, r->pelvis_target_height, cfg)
				- cfg.r_energy * energy * inv_act
				- cfg.r_smooth * smooth * inv_act;
			const auto done = episode_done(os, env.episode_steps, cfg.max_steps);

			mlp_forward(d.critic, env.obs_buf, env.vh1_buf, env.vh2_buf, std::span(&env.val_buf, 1));
			env.bootstrap = done ? 0.0f : env.val_buf;

			if (!d.buffer.full()) {
				const auto t = d.buffer.size;
				const auto obs_slot = std::span(d.buffer.obs).subspan(t * cfg.obs_dim, cfg.obs_dim);
				const auto act_slot = std::span(d.buffer.actions).subspan(t * cfg.act_dim, cfg.act_dim);
				std::ranges::copy(env.prev_obs_buf, obs_slot.begin());
				std::ranges::copy(env.action_buf, act_slot.begin());
				d.buffer.log_probs[t] = env.prev_log_prob;
				d.buffer.values[t] = env.prev_value;
				d.buffer.rewards[t] = reward;
				d.buffer.dones[t] = done ? 1.0f : 0.0f;
				d.buffer.env_of[t] = static_cast<std::uint32_t>(env.env_index);
				const auto pair_slot = std::span(d.buffer.amp_transitions).subspan(t * 2 * amp_feature_dim, 2 * amp_feature_dim);
				std::ranges::copy(env.prev_features, pair_slot.begin());
				std::ranges::copy(env.features, pair_slot.begin() + amp_feature_dim);
				++d.buffer.size;
				++d.total_steps;
			}

			env.episode_reward += reward;
			const auto err_fwd = (-os.velocity_body.z() - env.cmd_forward) / gse::meters_per_second(1.0f);
			const auto err_right = (os.velocity_body.x() - env.cmd_strafe) / gse::meters_per_second(1.0f);
			env.track_err_accum += std::sqrt(err_fwd * err_fwd + err_right * err_right);
			++env.episode_steps;

			if (done) {
				if (d.episode % 100 == 0) {
					gse::log::println(
						"locomotion_train: ep={} steps={} reward={:.2f} track_err={:.3f} total_steps={}",
						env.episode,
						env.episode_steps,
						env.episode_reward,
						env.track_err_accum / static_cast<float>(env.episode_steps),
						d.total_steps
					);
				}
				reset_episode(env, *r);
				continue;
			}
		}

		mlp_forward(d.actor.net, env.obs_buf, env.ah1_buf, env.ah2_buf, env.mean_buf);
		mlp_forward(d.critic, env.obs_buf, env.vh1_buf, env.vh2_buf, std::span(&env.val_buf, 1));

		std::ranges::copy(env.action_buf, env.prev_action_buf.begin());
		const auto log_prob = gaussian_sample(env.mean_buf, d.actor.log_std, env.action_buf, d.rng);
		if (cfg.action_scale == 1.0f) {
			apply_action(unpack_action(env.action_buf), *r, drives);
		}
		else {
			auto scaled = env.action_buf;
			for (auto& a : scaled) {
				a *= cfg.action_scale;
			}
			apply_action(unpack_action(scaled), *r, drives);
		}

		std::ranges::copy(env.obs_buf, env.prev_obs_buf.begin());
		env.prev_features = env.features;
		env.prev_log_prob = log_prob;
		env.prev_value = env.val_buf;
		env.has_prev = true;

		if (env.recover_grace > 0) {
			--env.recover_grace;
		}
		if (cfg.perturb_impulse > 0.0f && --env.perturb_timer <= 0) {
			auto angle_dist = std::uniform_real_distribution<float>(0.0f, 2.0f * std::numbers::pi_v<float>);
			const auto theta = angle_dist(d.rng);
			const auto mag = gse::newton_seconds(cfg.perturb_impulse);
			ctx.channels.push<gse::physics::impulse_request>({
				.target = r->pelvis_id,
				.impulse = gse::vec3<gse::impulse>(mag * std::cos(theta), gse::newton_seconds(0.0f), mag * std::sin(theta)),
			});
			env.perturb_timer = cfg.perturb_interval;
			env.recover_grace = cfg.perturb_grace;
		}

		if (env.rsi_active) {
			if (d.cycle_loop) {
				env.ref_index = env.ref_index >= d.cycle_end ? d.cycle_start : env.ref_index + 1;
			}
			else if (env.ref_index + 1 < d.ref_clip.frames.size()) {
				++env.ref_index;
			}
		}
	}

	if (d.buffer.full()) {
		auto boots = std::vector<float>{};
		boots.reserve(d.envs.size());
		for (const auto& env : d.envs) {
			boots.push_back(env.bootstrap);
		}
		update_amp_rewards(d, cfg);
		compute_gae(d.buffer, boots, cfg.gamma, cfg.lam);
		run_ppo_update(d, cfg);
		d.buffer.clear();
	}

	return {};
}

auto gs::locomotion::pose_player::run(data& d, const ppo_config& cfg, const gse::shared_view<gse::world_system::data> world_d, gse::read<skeleton_refs> refs, gse::read<state> states, gse::write<gse::physics::joint_drive_component> drives, gse::write<gse::physics::transform_component> transforms, gse::write<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (!d.attempted) {
		d.attempted = true;
		auto rng = std::mt19937(0u);
		d.actor = actor_make(cfg.obs_dim, cfg.act_dim, cfg.hidden_dim, rng);
		d.critic = mlp_make(cfg.obs_dim, cfg.hidden_dim, 1, 1.0f, rng);
		d.loaded = checkpoint_load(d.actor, d.critic, cfg.checkpoint_path);
		gse::log::println("pose_player: checkpoint '{}' {}", cfg.checkpoint_path, d.loaded ? "loaded" : "NOT found (humanoid stays passive)");
		d.rng.seed(1234u);
		if (!cfg.reference_clip_path.empty()) {
			if (auto clip = load_reference_clip(cfg.reference_clip_path)) {
				d.ref_clip = std::move(*clip);
				const auto we = extract_walk_frames(d.ref_clip, cfg.rsi_min_speed, cfg.rsi_max_speed);
				d.rsi_frames = we.frames;
				d.cycle_start = we.cycle_start;
				d.cycle_end = we.cycle_end;
				d.cycle_loop = we.cycle_loop;
				if (we.cycle_loop) {
					d.cmd_forward = we.cycle_speed / gse::meters_per_second(1.0f);
				}
				gse::log::println("pose_player: RSI {} walk frames (cycle_loop={}) cmd_forward={:.3f}", d.rsi_frames.size(), we.cycle_loop, d.cmd_forward);
			}
			else {
				gse::log::println("pose_player: RSI clip '{}' FAILED to load (neutral spawn)", cfg.reference_clip_path);
			}
		}
	}
	if (!d.loaded) {
		return {};
	}

	if (!d.scene_id.has_value()) {
		for (const auto& scene_id : world_d.scene_ids) {
			if (scene_id.tag() == std::string_view("Locomotion Play")) {
				d.scene_id = scene_id;
				break;
			}
		}
	}
	if (!d.scene_id.has_value() || world_d.active_scene != d.scene_id) {
		return {};
	}

	const auto sim_steps = gse::system_clock::fixed_steps_this_frame();
	if (sim_steps <= 0) {
		return {};
	}

	if (!world_d.active_scene_ptr) {
		return {};
	}
	const auto play_entities = world_d.active_scene_ptr->entities();

	const auto has_clip = !d.ref_clip.frames.empty();
	const auto do_reset = [&](owner_play_state& os, const skeleton_refs& r) {
		if (!d.rsi_frames.empty()) {
			auto pick = std::uniform_int_distribution<std::size_t>(0, d.rsi_frames.size() - 1);
			const auto fi = d.rsi_frames[pick(d.rng)];
			os.ref_index = fi;
			os.phi = d.ref_clip.frames[fi].kin.phi;
			reset_to_reference(os.initial_poses, r, d.ref_clip.frames[fi], transforms, motions);
			for (const auto& pose : os.initial_poses) {
				if (auto* mc = motions.find(pose.id)) {
					mc->current_velocity = {};
					mc->angular_velocity = {};
				}
			}
		}
		else {
			for (const auto& pose : os.initial_poses) {
				if (auto* tc = transforms.find(pose.id)) {
					tc->position = pose.position;
					tc->orientation = pose.orientation;
				}
				if (auto* mc = motions.find(pose.id)) {
					mc->current_velocity = {};
					mc->angular_velocity = {};
					mc->reset_pending = 1;
				}
			}
			os.ref_index = 0;
			os.phi = 0.0f;
		}
		os.reset_gate = 30;
	};

	const auto owner_ids = refs.owner_ids();
	if (!d.logged_owner_count) {
		auto in_scene = std::size_t{ 0 };
		for (const auto owner : owner_ids) {
			if (std::ranges::find(play_entities, owner) != play_entities.end()) {
				++in_scene;
			}
		}
		gse::log::println("pose_player: {} skeleton_refs owner(s) total, {} in the play scene (driving only those)", owner_ids.size(), in_scene);
		d.logged_owner_count = true;
	}
	for (std::size_t i = 0; i < owner_ids.size(); ++i) {
		const auto owner = owner_ids[i];
		if (std::ranges::find(play_entities, owner) == play_entities.end()) {
			continue;
		}
		const auto* r = refs.find(owner);
		const auto* s = states.find(owner);
		if (!r || !s || !s->valid) {
			continue;
		}

		auto& os = d.owners[owner.number()];
		if (os.obs_buf.empty()) {
			os.obs_buf.assign(cfg.obs_dim, 0.0f);
			os.h1_buf.assign(cfg.hidden_dim, 0.0f);
			os.h2_buf.assign(cfg.hidden_dim, 0.0f);
			os.mean_buf.assign(cfg.act_dim, 0.0f);
		}
		if (!os.snapshotted) {
			for (const auto bone_id : r->all_bone_ids) {
				if (const auto* tc = transforms.find(bone_id)) {
					os.initial_poses.push_back({ bone_id, tc->position, tc->orientation });
				}
			}
			os.snapshotted = true;
		}
		if (!os.drives_init) {
			initialize_drives(*r, drives);
			os.drives_init = true;
			do_reset(os, *r);
			continue;
		}

		if (os.reset_gate > 0) {
			auto hold = action{};
			if (has_clip) {
				const auto& rf = d.ref_clip.frames[os.ref_index];
				reset_to_reference(os.initial_poses, *r, rf, transforms, motions);
				for (const auto& pose : os.initial_poses) {
					if (auto* mc = motions.find(pose.id)) {
						mc->current_velocity = {};
						mc->angular_velocity = {};
					}
				}
				hold.hip_l_pitch = rf.kin.hip_angle_l;
				hold.knee_l = rf.kin.knee_angle_l;
				hold.ankle_l = rf.kin.ankle_angle_l;
				hold.hip_r_pitch = rf.kin.hip_angle_r;
				hold.knee_r = rf.kin.knee_angle_r;
				hold.ankle_r = rf.kin.ankle_angle_r;
			}
			apply_action(hold, *r, drives);
			if (s->pelvis_position.y() >= gse::meters(0.85f) && os.reset_gate < 27) {
				os.reset_gate = 0;
			}
			else {
				--os.reset_gate;
			}
			continue;
		}

		if (s->pelvis_position.y() < gse::meters(0.4f)) {
			gse::log::println("pose_player: owner={} fell after {} steps (pelvis_y={:.2f}) — RSI reset", owner.number(), os.steps_alive, s->pelvis_position.y());
			os.steps_alive = 0;
			do_reset(os, *r);
			apply_action(action{}, *r, drives);
			continue;
		}

		const auto cmd = intent{
			.forward = d.cmd_forward,
			.desired_yaw = gse::radians(0.f),
			.has_heading = true,
		};
		pack_observation(observe(*s, cmd, gait{}, 0.0f), os.obs_buf);
		mlp_forward(d.actor.net, os.obs_buf, os.h1_buf, os.h2_buf, os.mean_buf);
		apply_action(unpack_action(os.mean_buf), *r, drives);
		os.steps_alive += sim_steps;
		if (has_clip) {
			for (int k = 0; k < sim_steps; ++k) {
				if (d.cycle_loop) {
					os.ref_index = os.ref_index >= d.cycle_end ? d.cycle_start : os.ref_index + 1;
				}
				else {
					os.ref_index = os.ref_index + 1 < d.ref_clip.frames.size() ? os.ref_index + 1 : 0;
				}
			}
		}
		else {
			os.phi += static_cast<float>(sim_steps) / 88.0f;
			while (os.phi >= 1.0f) {
				os.phi -= 1.0f;
			}
		}
	}

	return {};
}
