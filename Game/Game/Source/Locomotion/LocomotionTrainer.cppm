export module gs:locomotion_trainer;

import std;
import gse;

import :locomotion_types;
import :locomotion_mdp;
import :locomotion_nn;

export namespace gs::locomotion {
	struct ppo_config {
		std::size_t obs_dim = 30;
		std::size_t act_dim = 10;
		std::size_t hidden_dim = 128;
		std::size_t n_envs = 32;
		std::size_t rollout_steps = 1024;
		std::size_t mini_batch = 256;
		int n_epochs = 10;
		float lr = 3e-4f;
		float gamma = 0.99f;
		float lam = 0.95f;
		float clip_eps = 0.2f;
		float value_coeff = 0.5f;
		float entropy_coeff = 0.01f;
		int max_steps = 400;
		std::string checkpoint_path = "locomotion_checkpoint.bin";
		int checkpoint_every = 50;
		unsigned seed = 1234;
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
		std::size_t capacity = 0;
		std::size_t size = 0;
		std::size_t obs_dim = 0;
		std::size_t act_dim = 0;

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
		int episode = 0;
		int episode_steps = 0;
		float episode_reward = 0.0f;
		float track_err_accum = 0.0f;
		float prev_value = 0.0f;
		float prev_log_prob = 0.0f;
		std::vector<body_pose> initial_poses;
		std::vector<float> obs_buf;
		std::vector<float> prev_obs_buf;
		std::vector<float> action_buf;
		std::vector<float> mean_buf;
		std::vector<float> ah1_buf;
		std::vector<float> ah2_buf;
		std::vector<float> vh1_buf;
		std::vector<float> vh2_buf;
		float val_buf = 0.0f;
	};

	struct trainer {
		struct data {
			ppo_config ppo;
			std::mt19937 rng;

			actor_params actor;
			mlp critic;
			actor_adam actor_opt;
			critic_adam critic_opt;

			rollout_buffer buffer;

			train_stage stage = train_stage::locating_scene;
			std::optional<gse::id> scene_id;
			int total_steps = 0;
			int update_count = 0;
			int episode = 0;

			std::vector<env_state> envs;

			explicit data(
				ppo_config cfg = {}
			);
		};

		static auto run(
			gse::context& ctx,
			data& d,
			gse::shared_view<gse::world_system> world_d,
			gse::read<skeleton_refs> refs,
			gse::read<state> states,
			gse::write<gse::physics::joint_drive_component> drives,
			gse::write<gse::physics::transform_component> transforms,
			gse::write<gse::physics::motion_component> motions
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	auto compute_gae(
		rollout_buffer& buf,
		std::span<const float> env_bootstrap,
		float gamma,
		float lam
	) -> void;

	auto run_ppo_update(
		trainer::data& d
	) -> void;

	auto initialize_drives(
		const skeleton_refs& r,
		gse::write<gse::physics::joint_drive_component>& drives
	) -> void;

	auto compute_reward(
		const state& s,
		gse::velocity cmd_forward,
		gse::velocity cmd_strafe
	) -> float;

	auto sample_command(
		env_state& env,
		std::mt19937& rng
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

	auto reset_env(
		env_state& env,
		const skeleton_refs& r,
		gse::write<gse::physics::transform_component>& transforms,
		gse::write<gse::physics::motion_component>& motions,
		gse::write<gse::physics::joint_drive_component>& drives
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
	, capacity(cap)
	, obs_dim(od)
	, act_dim(ad) {}

auto gs::locomotion::rollout_buffer::full() const -> bool {
	return size >= capacity;
}

auto gs::locomotion::rollout_buffer::clear() -> void {
	size = 0;
}

gs::locomotion::trainer::data::data(ppo_config cfg)
	: ppo(cfg)
	, rng(cfg.seed)
	, actor(actor_make(cfg.obs_dim, cfg.act_dim, cfg.hidden_dim, rng))
	, critic(mlp_make(cfg.obs_dim, cfg.hidden_dim, 1, 1.0f, rng))
	, actor_opt(actor_adam_make(actor))
	, critic_opt(critic_adam_make(critic))
	, buffer(cfg.rollout_steps, cfg.obs_dim, cfg.act_dim) {}

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

auto gs::locomotion::run_ppo_update(trainer::data& d) -> void {
	const auto& cfg = d.ppo;
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

	if (d.update_count % d.ppo.checkpoint_every == 0) {
		checkpoint_save(d.actor, d.critic, d.ppo.checkpoint_path);
	}
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

auto gs::locomotion::compute_reward(const state& s, const gse::velocity cmd_forward, const gse::velocity cmd_strafe) -> float {
	const auto pitch_limit = gse::degrees(45.f);
	const auto ref_speed = gse::meters_per_second(1.0f);
	const auto heading_limit = gse::radians(0.6f);
	constexpr auto track_weight = 2.0f;
	constexpr auto upright_weight = 0.5f;
	constexpr auto heading_weight = 0.3f;
	constexpr auto alive_bonus = 0.1f;

	const auto forward_speed = -s.velocity_body.z();
	const auto right_speed = s.velocity_body.x();
	const auto err_forward = (forward_speed - cmd_forward) / ref_speed;
	const auto err_right = (right_speed - cmd_strafe) / ref_speed;
	const auto track = std::exp(-4.0f * (err_forward * err_forward + err_right * err_right));

	const auto pitch_ratio = s.pelvis_pitch / pitch_limit;
	const auto upright = std::max(0.0f, 1.0f - std::abs(pitch_ratio));

	const auto current_yaw = std::atan2(-s.pelvis_forward.x(), -s.pelvis_forward.z());
	const auto heading_ratio = gse::radians(current_yaw) / heading_limit;
	const auto heading = std::exp(-heading_ratio * heading_ratio);

	return track_weight * track + upright_weight * upright + heading_weight * heading + alive_bonus;
}

auto gs::locomotion::episode_done(const state& s, const int steps, const int max_steps) -> bool {
	const gse::position fall_height = gse::meters(0.4f);
	return s.pelvis_position.y() < fall_height || steps >= max_steps;
}

auto gs::locomotion::sample_command(env_state& env, std::mt19937& rng) -> void {
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

auto gs::locomotion::reset_env(env_state& env, const skeleton_refs& r, gse::write<gse::physics::transform_component>& transforms, gse::write<gse::physics::motion_component>& motions, gse::write<gse::physics::joint_drive_component>& drives) -> void {
	for (const auto& pose : env.initial_poses) {
		if (auto* tc = transforms.find(pose.id)) {
			tc->position = pose.position;
			tc->orientation = pose.orientation;
		}
		if (auto* mc = motions.find(pose.id)) {
			mc->current_velocity = {};
			mc->angular_velocity = {};
		}
	}
	apply_action(action{}, r, drives);
	env.drives_initialized = false;
	env.has_prev = false;
	env.episode_steps = 0;
	env.episode_reward = 0.0f;
	env.track_err_accum = 0.0f;
}

auto gs::locomotion::trainer::run(gse::context& ctx, data& d, const gse::shared_view<gse::world_system> world_d, gse::read<skeleton_refs> refs, gse::read<state> states, gse::write<gse::physics::joint_drive_component> drives, gse::write<gse::physics::transform_component> transforms, gse::write<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (!d.scene_id.has_value()) {
		for (const auto& [scene_id, scene_ptr] : world_d.scenes) {
			if (scene_ptr && scene_ptr->id().tag() == std::string_view("Training")) {
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
				d.envs.push_back(make_env_state(owner_ids[i], i, d.ppo));
			}
			d.stage = train_stage::running;
			gse::log::println("locomotion_train: {} envs discovered, begin training", d.envs.size());
		}
		return {};
	}

	for (auto& env : d.envs) {
		const auto* r = refs.find(env.owner_id);
		const auto* s = states.find(env.owner_id);
		if (!r || !s || !s->valid) {
			continue;
		}

		if (!env.snapshot_taken) {
			take_env_snapshot(env, *r, transforms);
		}

		if (!env.drives_initialized) {
			initialize_drives(*r, drives);
			sample_command(env, d.rng);
			env.drives_initialized = true;
			continue;
		}

		auto cmd = intent{};
		cmd.forward = env.cmd_forward / gse::meters_per_second(1.0f);
		cmd.strafe = env.cmd_strafe / gse::meters_per_second(1.0f);
		cmd.has_heading = true;
		cmd.desired_yaw = gse::radians(0.f);
		pack_observation(observe(*s, cmd, gait{}, 0.0f), env.obs_buf);

		if (env.has_prev) {
			const auto reward = compute_reward(*s, env.cmd_forward, env.cmd_strafe);
			const auto done = episode_done(*s, env.episode_steps, d.ppo.max_steps);

			mlp_forward(d.critic, env.obs_buf, env.vh1_buf, env.vh2_buf, std::span(&env.val_buf, 1));
			env.bootstrap = done ? 0.0f : env.val_buf;

			if (!d.buffer.full()) {
				const auto t = d.buffer.size;
				const auto obs_slot = std::span(d.buffer.obs).subspan(t * d.ppo.obs_dim, d.ppo.obs_dim);
				const auto act_slot = std::span(d.buffer.actions).subspan(t * d.ppo.act_dim, d.ppo.act_dim);
				std::ranges::copy(env.prev_obs_buf, obs_slot.begin());
				std::ranges::copy(env.action_buf, act_slot.begin());
				d.buffer.log_probs[t] = env.prev_log_prob;
				d.buffer.values[t] = env.prev_value;
				d.buffer.rewards[t] = reward;
				d.buffer.dones[t] = done ? 1.0f : 0.0f;
				d.buffer.env_of[t] = static_cast<std::uint32_t>(env.env_index);
				++d.buffer.size;
				++d.total_steps;
			}

			env.episode_reward += reward;
			const auto err_fwd = (-s->velocity_body.z() - env.cmd_forward) / gse::meters_per_second(1.0f);
			const auto err_right = (s->velocity_body.x() - env.cmd_strafe) / gse::meters_per_second(1.0f);
			env.track_err_accum += std::sqrt(err_fwd * err_fwd + err_right * err_right);
			++env.episode_steps;

			if (done) {
				gse::log::println(
					"locomotion_train: ep={} steps={} reward={:.2f} track_err={:.3f} total_steps={}",
					env.episode,
					env.episode_steps,
					env.episode_reward,
					env.track_err_accum / static_cast<float>(env.episode_steps),
					d.total_steps
				);
				reset_env(env, *r, transforms, motions, drives);
				sample_command(env, d.rng);
				++env.episode;
				++d.episode;
				continue;
			}
		}

		mlp_forward(d.actor.net, env.obs_buf, env.ah1_buf, env.ah2_buf, env.mean_buf);
		mlp_forward(d.critic, env.obs_buf, env.vh1_buf, env.vh2_buf, std::span(&env.val_buf, 1));

		const auto log_prob = gaussian_sample(env.mean_buf, d.actor.log_std, env.action_buf, d.rng);
		apply_action(unpack_action(env.action_buf), *r, drives);

		std::ranges::copy(env.obs_buf, env.prev_obs_buf.begin());
		env.prev_log_prob = log_prob;
		env.prev_value = env.val_buf;
		env.has_prev = true;
	}

	if (d.buffer.full()) {
		auto boots = std::vector<float>{};
		boots.reserve(d.envs.size());
		for (const auto& env : d.envs) {
			boots.push_back(env.bootstrap);
		}
		compute_gae(d.buffer, boots, d.ppo.gamma, d.ppo.lam);
		run_ppo_update(d);
		d.buffer.clear();
	}

	return {};
}
