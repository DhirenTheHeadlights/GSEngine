export module gs:locomotion_nn;

import std;

export namespace gs::locomotion {
	struct nn_layer {
		std::size_t in_features = 0;
		std::size_t out_features = 0;
		std::vector<float> weight;
		std::vector<float> bias;
	};

	struct mlp {
		nn_layer l1, l2, l3;
	};

	struct actor_params {
		mlp net;
		std::vector<float> log_std;
	};

	struct adam_layer_state {
		std::vector<float> m_weight, v_weight;
		std::vector<float> m_bias, v_bias;
	};

	struct actor_adam {
		adam_layer_state l1, l2, l3;
		std::vector<float> m_log_std, v_log_std;
		int step = 0;
	};

	struct critic_adam {
		adam_layer_state l1, l2, l3;
		int step = 0;
	};

	auto nn_layer_make(
		std::size_t in_features,
		std::size_t out_features,
		float init_scale,
		std::mt19937& rng
	) -> nn_layer;

	auto mlp_make(
		std::size_t in_dim,
		std::size_t hidden,
		std::size_t out_dim,
		float out_scale,
		std::mt19937& rng
	) -> mlp;

	auto actor_make(
		std::size_t obs_dim,
		std::size_t act_dim,
		std::size_t hidden,
		std::mt19937& rng
	) -> actor_params;

	auto actor_adam_make(
		const actor_params& a
	) -> actor_adam;

	auto critic_adam_make(
		const mlp& critic
	) -> critic_adam;

	auto mlp_forward(
		const mlp& m,
		std::span<const float> input,
		std::span<float> h1,
		std::span<float> h2,
		std::span<float> output
	) -> void;

	auto gaussian_sample(
		std::span<const float> mean,
		std::span<const float> log_std,
		std::span<float> action,
		std::mt19937& rng
	) -> float;

	auto gaussian_logprob(
		std::span<const float> action,
		std::span<const float> mean,
		std::span<const float> log_std
	) -> float;

	auto gaussian_entropy(
		std::span<const float> log_std
	) -> float;

	auto ppo_actor_update(
		actor_params& actor,
		actor_adam& opt,
		std::mdspan<const float, std::dextents<std::size_t, 2>> obs,
		std::mdspan<const float, std::dextents<std::size_t, 2>> actions,
		std::span<const float> old_log_probs,
		std::span<const float> advantages,
		float lr,
		float clip_eps,
		float entropy_coeff
	) -> float;

	auto ppo_critic_update(
		mlp& critic,
		critic_adam& opt,
		std::mdspan<const float, std::dextents<std::size_t, 2>> obs,
		std::span<const float> returns,
		float lr,
		float value_coeff
	) -> float;

	auto checkpoint_save(
		const actor_params& actor,
		const mlp& critic,
		std::string_view path
	) -> bool;

	auto checkpoint_load(
		actor_params& actor,
		mlp& critic,
		std::string_view path
	) -> bool;
}

namespace gs::locomotion {
	auto linear(const nn_layer& l, std::span<const float> in, std::span<float> out) -> void {
		for (std::size_t o = 0; o < l.out_features; ++o) {
			auto acc = l.bias[o];
			const auto* w = l.weight.data() + o * l.in_features;
			for (std::size_t i = 0; i < l.in_features; ++i) {
				acc += w[i] * in[i];
			}
			out[o] = acc;
		}
	}

	auto linear_transpose(const nn_layer& l, std::span<const float> in, std::span<float> out) -> void {
		std::ranges::fill(out, 0.0f);
		for (std::size_t o = 0; o < l.out_features; ++o) {
			const auto* w = l.weight.data() + o * l.in_features;
			const auto vo = in[o];
			for (std::size_t i = 0; i < l.in_features; ++i) {
				out[i] += w[i] * vo;
			}
		}
	}

	auto rank1_add(std::span<const float> a, std::span<const float> b, std::span<float> mat) -> void {
		for (std::size_t i = 0; i < a.size(); ++i) {
			auto* row = mat.data() + i * b.size();
			const auto ai = a[i];
			for (std::size_t k = 0; k < b.size(); ++k) {
				row[k] += ai * b[k];
			}
		}
	}

	auto adam_step(std::vector<float>& m, std::vector<float>& v, std::span<float> params, std::span<const float> grads, int step, float lr) -> void {
		constexpr auto beta1 = 0.9f;
		constexpr auto beta2 = 0.999f;
		constexpr auto eps = 1e-8f;
		const auto bc1 = 1.0f - std::pow(beta1, static_cast<float>(step));
		const auto bc2 = 1.0f - std::pow(beta2, static_cast<float>(step));
		for (std::size_t i = 0; i < params.size(); ++i) {
			m[i] = beta1 * m[i] + (1.0f - beta1) * grads[i];
			v[i] = beta2 * v[i] + (1.0f - beta2) * grads[i] * grads[i];
			params[i] -= lr * (m[i] / bc1) / (std::sqrt(v[i] / bc2) + eps);
		}
	}

	auto save_span(std::ofstream& out, std::span<const float> data) -> void {
		const auto sz = data.size();
		out.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
		out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(sz * sizeof(float)));
	}

	auto load_vec(std::ifstream& in, std::vector<float>& vec) -> bool {
		std::size_t sz = 0;
		if (!in.read(reinterpret_cast<char*>(&sz), sizeof(sz))) {
			return false;
		}
		vec.resize(sz);
		return !!in.read(reinterpret_cast<char*>(vec.data()), static_cast<std::streamsize>(sz * sizeof(float)));
	}
}

auto gs::locomotion::nn_layer_make(const std::size_t in_features, const std::size_t out_features, const float init_scale, std::mt19937& rng) -> nn_layer {
	auto layer = nn_layer{
		.in_features = in_features,
		.out_features = out_features,
	};
	layer.weight.resize(out_features * in_features);
	layer.bias.resize(out_features, 0.0f);
	auto dist = std::normal_distribution<float>(0.0f, init_scale);
	for (auto& val : layer.weight) {
		val = dist(rng);
	}
	return layer;
}

auto gs::locomotion::mlp_make(const std::size_t in_dim, const std::size_t hidden, const std::size_t out_dim, const float out_scale, std::mt19937& rng) -> mlp {
	return {
		.l1 = nn_layer_make(in_dim, hidden, std::sqrt(2.0f / static_cast<float>(in_dim)), rng),
		.l2 = nn_layer_make(hidden, hidden, std::sqrt(2.0f / static_cast<float>(hidden)), rng),
		.l3 = nn_layer_make(hidden, out_dim, out_scale, rng),
	};
}

auto gs::locomotion::actor_make(const std::size_t obs_dim, const std::size_t act_dim, const std::size_t hidden, std::mt19937& rng) -> actor_params {
	return {
		.net = mlp_make(obs_dim, hidden, act_dim, 0.01f, rng),
		.log_std = std::vector<float>(act_dim, -1.5f),
	};
}

auto gs::locomotion::actor_adam_make(const actor_params& a) -> actor_adam {
	const auto make_ls = [](const nn_layer& l) -> adam_layer_state {
		return {
			.m_weight = std::vector<float>(l.out_features * l.in_features, 0.0f),
			.v_weight = std::vector<float>(l.out_features * l.in_features, 0.0f),
			.m_bias = std::vector<float>(l.out_features, 0.0f),
			.v_bias = std::vector<float>(l.out_features, 0.0f),
		};
	};
	return {
		.l1 = make_ls(a.net.l1),
		.l2 = make_ls(a.net.l2),
		.l3 = make_ls(a.net.l3),
		.m_log_std = std::vector<float>(a.log_std.size(), 0.0f),
		.v_log_std = std::vector<float>(a.log_std.size(), 0.0f),
	};
}

auto gs::locomotion::critic_adam_make(const mlp& critic) -> critic_adam {
	const auto make_ls = [](const nn_layer& l) -> adam_layer_state {
		return {
			.m_weight = std::vector<float>(l.out_features * l.in_features, 0.0f),
			.v_weight = std::vector<float>(l.out_features * l.in_features, 0.0f),
			.m_bias = std::vector<float>(l.out_features, 0.0f),
			.v_bias = std::vector<float>(l.out_features, 0.0f),
		};
	};
	return {
		.l1 = make_ls(critic.l1),
		.l2 = make_ls(critic.l2),
		.l3 = make_ls(critic.l3),
	};
}

auto gs::locomotion::mlp_forward(const mlp& m, const std::span<const float> input, const std::span<float> h1, const std::span<float> h2, const std::span<float> output) -> void {
	linear(m.l1, input, h1);
	std::ranges::transform(h1, h1.begin(), [](const float x) {
		return std::tanh(x);
	});
	linear(m.l2, h1, h2);
	std::ranges::transform(h2, h2.begin(), [](const float x) {
		return std::tanh(x);
	});
	linear(m.l3, h2, output);
}

auto gs::locomotion::gaussian_sample(const std::span<const float> mean, const std::span<const float> log_std, const std::span<float> action, std::mt19937& rng) -> float {
	auto norm = std::normal_distribution<float>{};
	auto log_prob = 0.0f;
	for (std::size_t i = 0; i < mean.size(); ++i) {
		const auto sigma = std::exp(log_std[i]);
		const auto eps_i = norm(rng);
		action[i] = mean[i] + sigma * eps_i;
		log_prob += -0.5f * eps_i * eps_i - log_std[i] - 0.5f * std::log(2.0f * std::numbers::pi_v<float>);
	}
	return log_prob;
}

auto gs::locomotion::gaussian_logprob(const std::span<const float> action, const std::span<const float> mean, const std::span<const float> log_std) -> float {
	auto log_prob = 0.0f;
	for (std::size_t i = 0; i < action.size(); ++i) {
		const auto sigma = std::exp(log_std[i]);
		const auto diff = (action[i] - mean[i]) / sigma;
		log_prob += -0.5f * diff * diff - log_std[i] - 0.5f * std::log(2.0f * std::numbers::pi_v<float>);
	}
	return log_prob;
}

auto gs::locomotion::gaussian_entropy(const std::span<const float> log_std) -> float {
	auto ent = 0.0f;
	for (const auto ls : log_std) {
		ent += ls + 0.5f * std::log(2.0f * std::numbers::pi_v<float> * std::numbers::e_v<float>);
	}
	return ent;
}

auto gs::locomotion::ppo_actor_update(actor_params& actor, actor_adam& opt, const std::mdspan<const float, std::dextents<std::size_t, 2>> obs, const std::mdspan<const float, std::dextents<std::size_t, 2>> actions, const std::span<const float> old_log_probs, const std::span<const float> advantages, const float lr, const float clip_eps, const float entropy_coeff) -> float {
	const auto n = obs.extent(0);
	const auto act_dim = actions.extent(1);
	const auto hd = actor.net.l1.out_features;

	auto h1_buf = std::vector<float>(n * hd);
	auto h2_buf = std::vector<float>(n * hd);
	auto mean_buf = std::vector<float>(n * act_dim);

	const auto h1_2d = std::mdspan(h1_buf.data(), n, hd);
	const auto h2_2d = std::mdspan(h2_buf.data(), n, hd);
	const auto mean_2d = std::mdspan(mean_buf.data(), n, act_dim);

	for (std::size_t s = 0; s < n; ++s) {
		const auto obs_s = std::submdspan(obs, s, std::full_extent);
		const auto h1_s = std::submdspan(h1_2d, s, std::full_extent);
		const auto h2_s = std::submdspan(h2_2d, s, std::full_extent);
		const auto mean_s = std::submdspan(mean_2d, s, std::full_extent);
		mlp_forward(
			actor.net,
			std::span(obs_s.data_handle(), obs_s.extent(0)),
			std::span(h1_s.data_handle(), hd),
			std::span(h2_s.data_handle(), hd),
			std::span(mean_s.data_handle(), act_dim)
		);
	}

	auto dW1 = std::vector<float>(hd * actor.net.l1.in_features, 0.0f);
	auto db1 = std::vector<float>(hd, 0.0f);
	auto dW2 = std::vector<float>(hd * hd, 0.0f);
	auto db2 = std::vector<float>(hd, 0.0f);
	auto dW3 = std::vector<float>(act_dim * hd, 0.0f);
	auto db3 = std::vector<float>(act_dim, 0.0f);
	auto dlog_std = std::vector<float>(act_dim, 0.0f);

	auto total_loss = 0.0f;

	for (std::size_t s = 0; s < n; ++s) {
		const auto obs_s = std::submdspan(obs, s, std::full_extent);
		const auto action_s = std::submdspan(actions, s, std::full_extent);
		const auto h1_s = std::submdspan(h1_2d, s, std::full_extent);
		const auto h2_s = std::submdspan(h2_2d, s, std::full_extent);
		const auto mean_s = std::submdspan(mean_2d, s, std::full_extent);
		const auto adv = advantages[s];

		const auto obs_sp = std::span(obs_s.data_handle(), obs_s.extent(0));
		const auto action_sp = std::span(action_s.data_handle(), act_dim);
		const auto h1_sp = std::span(h1_s.data_handle(), hd);
		const auto h2_sp = std::span(h2_s.data_handle(), hd);
		const auto mean_sp = std::span(mean_s.data_handle(), act_dim);
		const auto log_std_sp = std::span(actor.log_std);

		const auto new_lp = gaussian_logprob(action_sp, mean_sp, log_std_sp);
		const auto ratio = std::exp(new_lp - old_log_probs[s]);
		const auto clipped = std::clamp(ratio, 1.0f - clip_eps, 1.0f + clip_eps);
		const auto not_clipped = (adv >= 0.0f) ? (ratio < 1.0f + clip_eps) : (ratio > 1.0f - clip_eps);
		const auto d_logp = not_clipped ? -ratio * adv : 0.0f;

		total_loss += (not_clipped ? -ratio * adv : -clipped * adv) - entropy_coeff * gaussian_entropy(log_std_sp);

		auto dmean = std::vector<float>(act_dim);
		for (std::size_t i = 0; i < act_dim; ++i) {
			const auto sigma = std::exp(actor.log_std[i]);
			dmean[i] = d_logp * (action_sp[i] - mean_sp[i]) / (sigma * sigma);
		}

		for (std::size_t i = 0; i < act_dim; ++i) {
			const auto sigma = std::exp(actor.log_std[i]);
			const auto diff = (action_sp[i] - mean_sp[i]) / sigma;
			dlog_std[i] += d_logp * (diff * diff - 1.0f) - entropy_coeff;
		}

		rank1_add(dmean, h2_sp, dW3);
		std::ranges::transform(db3, dmean, db3.begin(), std::plus{});

		auto dh2 = std::vector<float>(hd, 0.0f);
		linear_transpose(actor.net.l3, dmean, dh2);

		auto dh2_pre = std::vector<float>(hd);
		std::ranges::transform(dh2, h2_sp, dh2_pre.begin(), [](const float d, const float h) {
			return d * (1.0f - h * h);
		});

		rank1_add(dh2_pre, h1_sp, dW2);
		std::ranges::transform(db2, dh2_pre, db2.begin(), std::plus{});

		auto dh1 = std::vector<float>(hd, 0.0f);
		linear_transpose(actor.net.l2, dh2_pre, dh1);

		auto dh1_pre = std::vector<float>(hd);
		std::ranges::transform(dh1, h1_sp, dh1_pre.begin(), [](const float d, const float h) {
			return d * (1.0f - h * h);
		});

		rank1_add(dh1_pre, obs_sp, dW1);
		std::ranges::transform(db1, dh1_pre, db1.begin(), std::plus{});
	}

	const auto inv_n = 1.0f / static_cast<float>(n);
	for (auto& val : dW1) {
		val *= inv_n;
	}
	for (auto& val : db1) {
		val *= inv_n;
	}
	for (auto& val : dW2) {
		val *= inv_n;
	}
	for (auto& val : db2) {
		val *= inv_n;
	}
	for (auto& val : dW3) {
		val *= inv_n;
	}
	for (auto& val : db3) {
		val *= inv_n;
	}
	for (auto& val : dlog_std) {
		val *= inv_n;
	}

	++opt.step;
	adam_step(opt.l1.m_weight, opt.l1.v_weight, actor.net.l1.weight, dW1, opt.step, lr);
	adam_step(opt.l1.m_bias, opt.l1.v_bias, actor.net.l1.bias, db1, opt.step, lr);
	adam_step(opt.l2.m_weight, opt.l2.v_weight, actor.net.l2.weight, dW2, opt.step, lr);
	adam_step(opt.l2.m_bias, opt.l2.v_bias, actor.net.l2.bias, db2, opt.step, lr);
	adam_step(opt.l3.m_weight, opt.l3.v_weight, actor.net.l3.weight, dW3, opt.step, lr);
	adam_step(opt.l3.m_bias, opt.l3.v_bias, actor.net.l3.bias, db3, opt.step, lr);
	adam_step(opt.m_log_std, opt.v_log_std, actor.log_std, dlog_std, opt.step, lr);

	return total_loss * inv_n;
}

auto gs::locomotion::ppo_critic_update(mlp& critic, critic_adam& opt, const std::mdspan<const float, std::dextents<std::size_t, 2>> obs, const std::span<const float> returns, const float lr, const float value_coeff) -> float {
	const auto n = obs.extent(0);
	const auto hd = critic.l1.out_features;

	auto h1_buf = std::vector<float>(n * hd);
	auto h2_buf = std::vector<float>(n * hd);
	auto val_buf = std::vector<float>(n);

	const auto h1_2d = std::mdspan(h1_buf.data(), n, hd);
	const auto h2_2d = std::mdspan(h2_buf.data(), n, hd);

	for (std::size_t s = 0; s < n; ++s) {
		const auto obs_s = std::submdspan(obs, s, std::full_extent);
		const auto h1_s = std::submdspan(h1_2d, s, std::full_extent);
		const auto h2_s = std::submdspan(h2_2d, s, std::full_extent);
		mlp_forward(
			critic,
			std::span(obs_s.data_handle(), obs_s.extent(0)),
			std::span(h1_s.data_handle(), hd),
			std::span(h2_s.data_handle(), hd),
			std::span(&val_buf[s], 1)
		);
	}

	auto dW1 = std::vector<float>(hd * critic.l1.in_features, 0.0f);
	auto db1 = std::vector<float>(hd, 0.0f);
	auto dW2 = std::vector<float>(hd * hd, 0.0f);
	auto db2 = std::vector<float>(hd, 0.0f);
	auto dW3 = std::vector<float>(hd, 0.0f);
	auto db3 = std::vector<float>(1, 0.0f);

	auto total_loss = 0.0f;

	for (std::size_t s = 0; s < n; ++s) {
		const auto obs_s = std::submdspan(obs, s, std::full_extent);
		const auto h1_s = std::submdspan(h1_2d, s, std::full_extent);
		const auto h2_s = std::submdspan(h2_2d, s, std::full_extent);
		const auto obs_sp = std::span(obs_s.data_handle(), obs_s.extent(0));
		const auto h1_sp = std::span(h1_s.data_handle(), hd);
		const auto h2_sp = std::span(h2_s.data_handle(), hd);

		const auto err = val_buf[s] - returns[s];
		total_loss += 0.5f * err * err;
		const auto dval = value_coeff * err;

		std::ranges::transform(dW3, h2_sp, dW3.begin(), [dval](const float dw, const float h) {
			return dw + dval * h;
		});
		db3[0] += dval;

		auto dh2 = std::vector<float>(hd);
		std::ranges::transform(critic.l3.weight, dh2.begin(), [dval](const float w) {
			return dval * w;
		});

		auto dh2_pre = std::vector<float>(hd);
		std::ranges::transform(dh2, h2_sp, dh2_pre.begin(), [](const float d, const float h) {
			return d * (1.0f - h * h);
		});

		rank1_add(dh2_pre, h1_sp, dW2);
		std::ranges::transform(db2, dh2_pre, db2.begin(), std::plus{});

		auto dh1 = std::vector<float>(hd, 0.0f);
		linear_transpose(critic.l2, dh2_pre, dh1);

		auto dh1_pre = std::vector<float>(hd);
		std::ranges::transform(dh1, h1_sp, dh1_pre.begin(), [](const float d, const float h) {
			return d * (1.0f - h * h);
		});

		rank1_add(dh1_pre, obs_sp, dW1);
		std::ranges::transform(db1, dh1_pre, db1.begin(), std::plus{});
	}

	const auto inv_n = 1.0f / static_cast<float>(n);
	for (auto& val : dW1) {
		val *= inv_n;
	}
	for (auto& val : db1) {
		val *= inv_n;
	}
	for (auto& val : dW2) {
		val *= inv_n;
	}
	for (auto& val : db2) {
		val *= inv_n;
	}
	for (auto& val : dW3) {
		val *= inv_n;
	}
	db3[0] *= inv_n;

	++opt.step;
	adam_step(opt.l1.m_weight, opt.l1.v_weight, critic.l1.weight, dW1, opt.step, lr);
	adam_step(opt.l1.m_bias, opt.l1.v_bias, critic.l1.bias, db1, opt.step, lr);
	adam_step(opt.l2.m_weight, opt.l2.v_weight, critic.l2.weight, dW2, opt.step, lr);
	adam_step(opt.l2.m_bias, opt.l2.v_bias, critic.l2.bias, db2, opt.step, lr);
	adam_step(opt.l3.m_weight, opt.l3.v_weight, critic.l3.weight, dW3, opt.step, lr);
	adam_step(opt.l3.m_bias, opt.l3.v_bias, critic.l3.bias, db3, opt.step, lr);

	return total_loss * inv_n;
}

auto gs::locomotion::checkpoint_save(const actor_params& actor, const mlp& critic, const std::string_view path) -> bool {
	auto out = std::ofstream(std::string(path), std::ios::binary);
	if (!out.is_open()) {
		return false;
	}
	save_span(out, actor.net.l1.weight);
	save_span(out, actor.net.l1.bias);
	save_span(out, actor.net.l2.weight);
	save_span(out, actor.net.l2.bias);
	save_span(out, actor.net.l3.weight);
	save_span(out, actor.net.l3.bias);
	save_span(out, actor.log_std);
	save_span(out, critic.l1.weight);
	save_span(out, critic.l1.bias);
	save_span(out, critic.l2.weight);
	save_span(out, critic.l2.bias);
	save_span(out, critic.l3.weight);
	save_span(out, critic.l3.bias);
	return true;
}

auto gs::locomotion::checkpoint_load(actor_params& actor, mlp& critic, const std::string_view path) -> bool {
	auto in = std::ifstream(std::string(path), std::ios::binary);
	if (!in.is_open()) {
		return false;
	}
	if (!load_vec(in, actor.net.l1.weight)) {
		return false;
	}
	if (!load_vec(in, actor.net.l1.bias)) {
		return false;
	}
	if (!load_vec(in, actor.net.l2.weight)) {
		return false;
	}
	if (!load_vec(in, actor.net.l2.bias)) {
		return false;
	}
	if (!load_vec(in, actor.net.l3.weight)) {
		return false;
	}
	if (!load_vec(in, actor.net.l3.bias)) {
		return false;
	}
	if (!load_vec(in, actor.log_std)) {
		return false;
	}
	if (!load_vec(in, critic.l1.weight)) {
		return false;
	}
	if (!load_vec(in, critic.l1.bias)) {
		return false;
	}
	if (!load_vec(in, critic.l2.weight)) {
		return false;
	}
	if (!load_vec(in, critic.l2.bias)) {
		return false;
	}
	if (!load_vec(in, critic.l3.weight)) {
		return false;
	}
	if (!load_vec(in, critic.l3.bias)) {
		return false;
	}
	return true;
}
