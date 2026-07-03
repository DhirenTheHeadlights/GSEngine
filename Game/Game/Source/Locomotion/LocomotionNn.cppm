export module gs:locomotion_nn;

import std;

import gse;

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

	struct train_meta {
		int update_count = 0;
		int total_steps = 0;
		float best_mean_surv = 0.0f;
	};

	auto checkpoint_save_full(
		const actor_params& actor,
		const actor_adam& actor_opt,
		const mlp& critic,
		const critic_adam& critic_opt,
		const mlp& discriminator,
		const critic_adam& disc_opt,
		const train_meta& meta,
		std::string_view path
	) -> bool;

	auto checkpoint_load_full(
		actor_params& actor,
		actor_adam& actor_opt,
		mlp& critic,
		critic_adam& critic_opt,
		mlp& discriminator,
		critic_adam& disc_opt,
		train_meta& meta,
		std::string_view path
	) -> bool;

	struct disc_metrics {
		float loss = 0.0f;
		float mean_real = 0.0f;
		float mean_fake = 0.0f;
		float r1 = 0.0f;
	};

	auto discriminator_update(
		mlp& net,
		critic_adam& opt,
		std::mdspan<const float, std::dextents<std::size_t, 2>> real,
		std::mdspan<const float, std::dextents<std::size_t, 2>> fake,
		float lr,
		float r1_weight,
		float gate_loss
	) -> disc_metrics;

	auto discriminator_style_reward(
		const mlp& net,
		std::span<const float> transition
	) -> float;

	auto discriminator_selftest() -> bool;
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

	constexpr std::uint32_t checkpoint_magic = 0x43484b50;
	constexpr std::uint32_t checkpoint_version = 1;

	auto discriminator_input_grad_norm_sq(const mlp& net, std::span<const float> x) -> float {
		const auto in_dim = net.l1.in_features;
		const auto hd = net.l1.out_features;
		auto a1 = std::vector<float>(hd);
		auto a2 = std::vector<float>(hd);
		linear(net.l1, x, a1);
		for (auto& v : a1) {
			v = std::tanh(v);
		}
		linear(net.l2, a1, a2);
		for (auto& v : a2) {
			v = std::tanh(v);
		}
		auto d_z2 = std::vector<float>(hd);
		for (std::size_t j = 0; j < hd; ++j) {
			d_z2[j] = net.l3.weight[j] * (1.0f - a2[j] * a2[j]);
		}
		auto d_a1 = std::vector<float>(hd, 0.0f);
		linear_transpose(net.l2, d_z2, d_a1);
		auto d_z1 = std::vector<float>(hd);
		for (std::size_t h = 0; h < hd; ++h) {
			d_z1[h] = d_a1[h] * (1.0f - a1[h] * a1[h]);
		}
		auto g = std::vector<float>(in_dim, 0.0f);
		linear_transpose(net.l1, d_z1, g);
		auto sumsq = 0.0f;
		for (const auto v : g) {
			sumsq += v * v;
		}
		return 0.5f * sumsq;
	}

	auto discriminator_r1_accumulate(const mlp& net, std::span<const float> x, std::span<float> dW1, std::span<float> db1, std::span<float> dW2, std::span<float> db2, std::span<float> dw3) -> float {
		const auto in_dim = net.l1.in_features;
		const auto hd = net.l1.out_features;
		auto a1 = std::vector<float>(hd);
		auto a2 = std::vector<float>(hd);
		linear(net.l1, x, a1);
		for (auto& v : a1) {
			v = std::tanh(v);
		}
		linear(net.l2, a1, a2);
		for (auto& v : a2) {
			v = std::tanh(v);
		}
		auto s1 = std::vector<float>(hd);
		auto s2 = std::vector<float>(hd);
		for (std::size_t i = 0; i < hd; ++i) {
			s1[i] = 1.0f - a1[i] * a1[i];
			s2[i] = 1.0f - a2[i] * a2[i];
		}
		auto d_z2 = std::vector<float>(hd);
		for (std::size_t j = 0; j < hd; ++j) {
			d_z2[j] = net.l3.weight[j] * s2[j];
		}
		auto d_a1 = std::vector<float>(hd, 0.0f);
		linear_transpose(net.l2, d_z2, d_a1);
		auto d_z1 = std::vector<float>(hd);
		for (std::size_t h = 0; h < hd; ++h) {
			d_z1[h] = d_a1[h] * s1[h];
		}
		auto g = std::vector<float>(in_dim, 0.0f);
		linear_transpose(net.l1, d_z1, g);

		auto penalty = 0.0f;
		for (const auto v : g) {
			penalty += v * v;
		}
		penalty *= 0.5f;

		for (std::size_t h = 0; h < hd; ++h) {
			const auto dz1h = d_z1[h];
			auto* row = dW1.data() + h * in_dim;
			for (std::size_t d = 0; d < in_dim; ++d) {
				row[d] += dz1h * g[d];
			}
		}
		auto d_z1_bar = std::vector<float>(hd, 0.0f);
		for (std::size_t h = 0; h < hd; ++h) {
			const auto* row = net.l1.weight.data() + h * in_dim;
			auto acc = 0.0f;
			for (std::size_t d = 0; d < in_dim; ++d) {
				acc += row[d] * g[d];
			}
			d_z1_bar[h] = acc;
		}
		auto d_a1_bar = std::vector<float>(hd);
		auto s1_bar = std::vector<float>(hd);
		for (std::size_t h = 0; h < hd; ++h) {
			d_a1_bar[h] = d_z1_bar[h] * s1[h];
			s1_bar[h] = d_z1_bar[h] * d_a1[h];
		}
		auto a1_bar = std::vector<float>(hd);
		for (std::size_t h = 0; h < hd; ++h) {
			a1_bar[h] = s1_bar[h] * (-2.0f * a1[h]);
		}
		for (std::size_t j = 0; j < hd; ++j) {
			const auto dz2j = d_z2[j];
			auto* row = dW2.data() + j * hd;
			for (std::size_t h = 0; h < hd; ++h) {
				row[h] += dz2j * d_a1_bar[h];
			}
		}
		auto d_z2_bar = std::vector<float>(hd, 0.0f);
		for (std::size_t j = 0; j < hd; ++j) {
			const auto* row = net.l2.weight.data() + j * hd;
			auto acc = 0.0f;
			for (std::size_t h = 0; h < hd; ++h) {
				acc += row[h] * d_a1_bar[h];
			}
			d_z2_bar[j] = acc;
		}
		for (std::size_t j = 0; j < hd; ++j) {
			dw3[j] += d_z2_bar[j] * s2[j];
		}
		auto z2_bar = std::vector<float>(hd);
		for (std::size_t j = 0; j < hd; ++j) {
			const auto s2_bar = d_z2_bar[j] * net.l3.weight[j];
			const auto a2_bar = s2_bar * (-2.0f * a2[j]);
			z2_bar[j] = a2_bar * s2[j];
		}
		for (std::size_t j = 0; j < hd; ++j) {
			const auto zb = z2_bar[j];
			auto* row = dW2.data() + j * hd;
			for (std::size_t h = 0; h < hd; ++h) {
				row[h] += zb * a1[h];
			}
			db2[j] += zb;
		}
		auto a1_bar_fwd = std::vector<float>(hd, 0.0f);
		linear_transpose(net.l2, z2_bar, a1_bar_fwd);
		for (std::size_t h = 0; h < hd; ++h) {
			const auto zb = (a1_bar[h] + a1_bar_fwd[h]) * s1[h];
			auto* row = dW1.data() + h * in_dim;
			for (std::size_t d = 0; d < in_dim; ++d) {
				row[d] += zb * x[d];
			}
			db1[h] += zb;
		}
		return penalty;
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

auto gs::locomotion::discriminator_update(mlp& net, critic_adam& opt, const std::mdspan<const float, std::dextents<std::size_t, 2>> real, const std::mdspan<const float, std::dextents<std::size_t, 2>> fake, const float lr, const float r1_weight, const float gate_loss) -> disc_metrics {
	const auto in_dim = net.l1.in_features;
	const auto hd = net.l1.out_features;
	const auto total = real.extent(0) + fake.extent(0);

	auto dW1 = std::vector<float>(hd * in_dim, 0.0f);
	auto db1 = std::vector<float>(hd, 0.0f);
	auto dW2 = std::vector<float>(hd * hd, 0.0f);
	auto db2 = std::vector<float>(hd, 0.0f);
	auto dW3 = std::vector<float>(hd, 0.0f);
	auto db3 = std::vector<float>(1, 0.0f);

	auto h1 = std::vector<float>(hd);
	auto h2 = std::vector<float>(hd);
	auto dh2_pre = std::vector<float>(hd);
	auto dh1 = std::vector<float>(hd);
	auto dh1_pre = std::vector<float>(hd);

	auto metrics = disc_metrics{};

	const auto accumulate = [&](const std::mdspan<const float, std::dextents<std::size_t, 2>> batch, const float target) -> float {
		const auto n = batch.extent(0);
		auto sum_y = 0.0f;
		for (std::size_t s = 0; s < n; ++s) {
			const auto row = std::submdspan(batch, s, std::full_extent);
			const auto input = std::span<const float>(row.data_handle(), in_dim);
			auto y = 0.0f;
			mlp_forward(net, input, h1, h2, std::span(&y, 1));
			sum_y += y;
			const auto dval = y - target;
			metrics.loss += 0.5f * dval * dval;

			for (std::size_t k = 0; k < hd; ++k) {
				dW3[k] += dval * h2[k];
			}
			db3[0] += dval;

			for (std::size_t j = 0; j < hd; ++j) {
				const auto dh2 = net.l3.weight[j] * dval;
				dh2_pre[j] = dh2 * (1.0f - h2[j] * h2[j]);
			}
			rank1_add(dh2_pre, h1, dW2);
			std::ranges::transform(db2, dh2_pre, db2.begin(), std::plus{});

			linear_transpose(net.l2, dh2_pre, dh1);
			for (std::size_t i = 0; i < hd; ++i) {
				dh1_pre[i] = dh1[i] * (1.0f - h1[i] * h1[i]);
			}
			rank1_add(dh1_pre, input, dW1);
			std::ranges::transform(db1, dh1_pre, db1.begin(), std::plus{});
		}
		return n > 0 ? sum_y / static_cast<float>(n) : 0.0f;
	};

	metrics.mean_real = accumulate(real, 1.0f);
	metrics.mean_fake = accumulate(fake, -1.0f);

	const auto inv = total > 0 ? 1.0f / static_cast<float>(total) : 0.0f;
	for (auto& v : dW1) {
		v *= inv;
	}
	for (auto& v : db1) {
		v *= inv;
	}
	for (auto& v : dW2) {
		v *= inv;
	}
	for (auto& v : db2) {
		v *= inv;
	}
	for (auto& v : dW3) {
		v *= inv;
	}
	db3[0] *= inv;
	metrics.loss *= inv;

	if (metrics.loss < gate_loss) {
		return metrics;
	}

	if (r1_weight > 0.0f) {
		const auto n_real = real.extent(0);
		auto rW1 = std::vector<float>(hd * in_dim, 0.0f);
		auto rb1 = std::vector<float>(hd, 0.0f);
		auto rW2 = std::vector<float>(hd * hd, 0.0f);
		auto rb2 = std::vector<float>(hd, 0.0f);
		auto rw3 = std::vector<float>(hd, 0.0f);
		auto penalty_sum = 0.0f;
		for (std::size_t s = 0; s < n_real; ++s) {
			const auto row = std::submdspan(real, s, std::full_extent);
			const auto input = std::span<const float>(row.data_handle(), in_dim);
			penalty_sum += discriminator_r1_accumulate(net, input, rW1, rb1, rW2, rb2, rw3);
		}
		const auto scale = n_real > 0 ? r1_weight / static_cast<float>(n_real) : 0.0f;
		for (std::size_t i = 0; i < dW1.size(); ++i) {
			dW1[i] += scale * rW1[i];
		}
		for (std::size_t i = 0; i < db1.size(); ++i) {
			db1[i] += scale * rb1[i];
		}
		for (std::size_t i = 0; i < dW2.size(); ++i) {
			dW2[i] += scale * rW2[i];
		}
		for (std::size_t i = 0; i < db2.size(); ++i) {
			db2[i] += scale * rb2[i];
		}
		for (std::size_t i = 0; i < dW3.size(); ++i) {
			dW3[i] += scale * rw3[i];
		}
		metrics.r1 = n_real > 0 ? penalty_sum / static_cast<float>(n_real) : 0.0f;
	}

	++opt.step;
	adam_step(opt.l1.m_weight, opt.l1.v_weight, net.l1.weight, dW1, opt.step, lr);
	adam_step(opt.l1.m_bias, opt.l1.v_bias, net.l1.bias, db1, opt.step, lr);
	adam_step(opt.l2.m_weight, opt.l2.v_weight, net.l2.weight, dW2, opt.step, lr);
	adam_step(opt.l2.m_bias, opt.l2.v_bias, net.l2.bias, db2, opt.step, lr);
	adam_step(opt.l3.m_weight, opt.l3.v_weight, net.l3.weight, dW3, opt.step, lr);
	adam_step(opt.l3.m_bias, opt.l3.v_bias, net.l3.bias, db3, opt.step, lr);

	return metrics;
}

auto gs::locomotion::discriminator_style_reward(const mlp& net, const std::span<const float> transition) -> float {
	const auto hd = net.l1.out_features;
	auto h1 = std::vector<float>(hd);
	auto h2 = std::vector<float>(hd);
	auto logit = 0.0f;
	mlp_forward(net, transition, h1, h2, std::span(&logit, 1));
	const auto shaped = 1.0f - 0.25f * (logit - 1.0f) * (logit - 1.0f);
	return std::max(0.0f, shaped);
}

auto gs::locomotion::discriminator_selftest() -> bool {
	auto ok = true;
	auto rng = std::mt19937(7777u);
	const auto in_dim = std::size_t{ 12 };
	const auto hidden = std::size_t{ 32 };
	const auto batch = std::size_t{ 64 };

	auto net = mlp_make(in_dim, hidden, 1, 0.1f, rng);
	auto opt = critic_adam_make(net);

	auto real_dist = std::normal_distribution<float>(1.0f, 0.3f);
	auto fake_dist = std::normal_distribution<float>(0.0f, 1.5f);

	auto real_buf = std::vector<float>(batch * in_dim);
	auto fake_buf = std::vector<float>(batch * in_dim);
	const auto fill = [&rng](std::vector<float>& buf, std::normal_distribution<float>& dist) {
		for (auto& v : buf) {
			v = dist(rng);
		}
	};

	const auto make_span = [batch, in_dim](const std::vector<float>& buf) {
		return std::mdspan<const float, std::dextents<std::size_t, 2>>(buf.data(), batch, in_dim);
	};

	auto first_loss = 0.0f;
	auto last = disc_metrics{};
	for (auto it = 0; it < 600; ++it) {
		fill(real_buf, real_dist);
		fill(fake_buf, fake_dist);
		last = discriminator_update(net, opt, make_span(real_buf), make_span(fake_buf), 1e-3f, 0.0f, 0.0f);
		if (it == 0) {
			first_loss = last.loss;
		}
	}

	const auto separation = last.mean_real - last.mean_fake;
	const auto sep_loss = last.loss;
	if (separation < 1.0f) {
		gse::log::println("discriminator_selftest: FAIL separation={:.3f} (real={:.3f} fake={:.3f})", separation, last.mean_real, last.mean_fake);
		ok = false;
	}
	if (sep_loss >= first_loss) {
		gse::log::println("discriminator_selftest: FAIL loss did not decrease ({:.3f} -> {:.3f})", first_loss, sep_loss);
		ok = false;
	}

	auto real_sample = std::vector<float>(in_dim, 1.0f);
	auto fake_sample = std::vector<float>(in_dim, 0.0f);
	const auto r_real = discriminator_style_reward(net, real_sample);
	const auto r_fake = discriminator_style_reward(net, fake_sample);
	if (r_real <= r_fake) {
		gse::log::println("discriminator_selftest: FAIL style reward not ordered (real={:.3f} fake={:.3f})", r_real, r_fake);
		ok = false;
	}

	auto worst = 0.0f;
	{
		auto grng = std::mt19937(2024u);
		const auto gdim = std::size_t{ 8 };
		const auto ghid = std::size_t{ 16 };
		auto gnet = mlp_make(gdim, ghid, 1, 0.6f, grng);
		auto coord = std::normal_distribution<float>(0.0f, 1.0f);
		auto x = std::vector<float>(gdim);
		for (auto& v : x) {
			v = coord(grng);
		}
		auto dW1 = std::vector<float>(ghid * gdim, 0.0f);
		auto db1 = std::vector<float>(ghid, 0.0f);
		auto dW2 = std::vector<float>(ghid * ghid, 0.0f);
		auto db2 = std::vector<float>(ghid, 0.0f);
		auto dw3 = std::vector<float>(ghid, 0.0f);
		discriminator_r1_accumulate(gnet, x, dW1, db1, dW2, db2, dw3);

		const auto eps = 1e-3f;
		const auto check = [&](std::vector<float>& param, std::span<const float> analytic) -> float {
			auto diff_sq = 0.0f;
			auto num_sq = 0.0f;
			auto ana_sq = 0.0f;
			for (std::size_t i = 0; i < param.size(); ++i) {
				const auto orig = param[i];
				param[i] = orig + eps;
				const auto pp = discriminator_input_grad_norm_sq(gnet, x);
				param[i] = orig - eps;
				const auto pm = discriminator_input_grad_norm_sq(gnet, x);
				param[i] = orig;
				const auto numeric = (pp - pm) / (2.0f * eps);
				const auto d = numeric - analytic[i];
				diff_sq += d * d;
				num_sq += numeric * numeric;
				ana_sq += analytic[i] * analytic[i];
			}
			return std::sqrt(diff_sq) / (std::sqrt(num_sq) + std::sqrt(ana_sq) + 1e-6f);
		};
		const auto e_w1 = check(gnet.l1.weight, dW1);
		const auto e_b1 = check(gnet.l1.bias, db1);
		const auto e_w2 = check(gnet.l2.weight, dW2);
		const auto e_b2 = check(gnet.l2.bias, db2);
		const auto e_w3 = check(gnet.l3.weight, dw3);
		worst = std::max({ e_w1, e_b1, e_w2, e_b2, e_w3 });
		gse::log::println("discriminator_selftest: R1 grad check (l2-rel) W1={:.5f} b1={:.5f} W2={:.5f} b2={:.5f} w3={:.5f}", e_w1, e_b1, e_w2, e_b2, e_w3);
		if (worst > 1e-2f) {
			ok = false;
		}
	}

	for (auto it = 0; it < 100; ++it) {
		fill(real_buf, real_dist);
		fill(fake_buf, fake_dist);
		last = discriminator_update(net, opt, make_span(real_buf), make_span(fake_buf), 1e-3f, 1.0f, 0.0f);
	}
	if (!std::isfinite(last.mean_real) || !std::isfinite(last.mean_fake) || !std::isfinite(last.r1)) {
		gse::log::println("discriminator_selftest: FAIL R1 update non-finite (real={} fake={} r1={})", last.mean_real, last.mean_fake, last.r1);
		ok = false;
	}

	gse::log::println("discriminator_selftest: {} sep={:.3f} loss {:.3f}->{:.3f} style={:.3f}/{:.3f} r1_grad_l2_rel={:.5f} r1_on(real={:.3f} fake={:.3f} r1={:.4f})", ok ? "PASS" : "FAIL", separation, first_loss, sep_loss, r_real, r_fake, worst, last.mean_real, last.mean_fake, last.r1);
	return ok;
}

auto gs::locomotion::checkpoint_save(const actor_params& actor, const mlp& critic, const std::string_view path) -> bool {
	auto out = std::ofstream(std::string(path), std::ios::binary);
	if (!out.is_open()) {
		return false;
	}
	auto ar = gse::binary_writer(out, checkpoint_magic, checkpoint_version);
	const auto obs = static_cast<std::uint32_t>(actor.net.l1.in_features);
	const auto act = static_cast<std::uint32_t>(actor.net.l3.out_features);
	const auto hidden = static_cast<std::uint32_t>(actor.net.l1.out_features);
	ar & obs & act & hidden & actor & critic;
	return static_cast<bool>(out);
}

auto gs::locomotion::checkpoint_load(actor_params& actor, mlp& critic, const std::string_view path) -> bool {
	auto in = std::ifstream(std::string(path), std::ios::binary);
	if (!in.is_open()) {
		return false;
	}
	auto ar = gse::binary_reader(in);
	auto magic = std::uint32_t{};
	auto version = std::uint32_t{};
	auto obs = std::uint32_t{};
	auto act = std::uint32_t{};
	auto hidden = std::uint32_t{};
	ar & magic & version & obs & act & hidden;
	if (magic != checkpoint_magic || version != checkpoint_version) {
		return false;
	}
	if (obs != actor.net.l1.in_features || act != actor.net.l3.out_features || hidden != actor.net.l1.out_features) {
		return false;
	}
	ar & actor & critic;
	return !in.fail();
}

auto gs::locomotion::checkpoint_save_full(const actor_params& actor, const actor_adam& actor_opt, const mlp& critic, const critic_adam& critic_opt, const mlp& discriminator, const critic_adam& disc_opt, const train_meta& meta, const std::string_view path) -> bool {
	auto out = std::ofstream(std::string(path), std::ios::binary);
	if (!out.is_open()) {
		return false;
	}
	auto ar = gse::binary_writer(out, checkpoint_magic, checkpoint_version);
	const auto obs = static_cast<std::uint32_t>(actor.net.l1.in_features);
	const auto act = static_cast<std::uint32_t>(actor.net.l3.out_features);
	const auto hidden = static_cast<std::uint32_t>(actor.net.l1.out_features);
	ar & obs & act & hidden & actor & actor_opt & critic & critic_opt & discriminator & disc_opt & meta;
	return static_cast<bool>(out);
}

auto gs::locomotion::checkpoint_load_full(actor_params& actor, actor_adam& actor_opt, mlp& critic, critic_adam& critic_opt, mlp& discriminator, critic_adam& disc_opt, train_meta& meta, const std::string_view path) -> bool {
	auto in = std::ifstream(std::string(path), std::ios::binary);
	if (!in.is_open()) {
		return false;
	}
	auto ar = gse::binary_reader(in);
	auto magic = std::uint32_t{};
	auto version = std::uint32_t{};
	auto obs = std::uint32_t{};
	auto act = std::uint32_t{};
	auto hidden = std::uint32_t{};
	ar & magic & version & obs & act & hidden;
	if (magic != checkpoint_magic || version != checkpoint_version) {
		return false;
	}
	if (obs != actor.net.l1.in_features || act != actor.net.l3.out_features || hidden != actor.net.l1.out_features) {
		return false;
	}
	ar & actor & actor_opt & critic & critic_opt & discriminator & disc_opt & meta;
	return !in.fail();
}
