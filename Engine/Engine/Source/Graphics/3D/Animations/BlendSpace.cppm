export module gse.graphics:blend_space;

import std;

import :animation_components;
import :clip;

import gse.core;
import gse.math;
import gse.assets;

export namespace gse::animation {
	enum class locomotion_direction : std::uint8_t {
		forward = 0,
		backward = 1,
		left = 2,
		right = 3,
	};

	struct locomotion_blend {
		static constexpr std::size_t direction_count = 4;

		resource::handle<clip_asset> idle{};
		std::array<resource::handle<clip_asset>, direction_count> walk{};
		std::array<resource::handle<clip_asset>, direction_count> run{};
	};

	auto direction_weights(
		vec2f input
	) -> std::array<float, locomotion_blend::direction_count>;

	auto blend_weights(
		const locomotion_blend& layout,
		vec2f input,
		float tier,
		std::span<clip_layer> out
	) -> std::uint32_t;
}

auto gse::animation::direction_weights(const vec2f input) -> std::array<float, locomotion_blend::direction_count> {
	std::array<float, locomotion_blend::direction_count> weights{};

	const auto unit = normalize(input);
	if (is_zero(unit)) {
		return weights;
	}

	weights[static_cast<std::size_t>(locomotion_direction::forward)] = std::max(0.f, unit.y());
	weights[static_cast<std::size_t>(locomotion_direction::backward)] = std::max(0.f, -unit.y());
	weights[static_cast<std::size_t>(locomotion_direction::left)] = std::max(0.f, -unit.x());
	weights[static_cast<std::size_t>(locomotion_direction::right)] = std::max(0.f, unit.x());

	const float total = std::accumulate(weights.begin(), weights.end(), 0.f);
	if (total > 0.f) {
		for (float& w : weights) {
			w /= total;
		}
	}
	return weights;
}

auto gse::animation::blend_weights(const locomotion_blend& layout, const vec2f input, const float tier, const std::span<clip_layer> out) -> std::uint32_t {
	const float throttle = std::clamp(magnitude(input), 0.f, 1.f);
	const float run_share = std::clamp(tier, 0.f, 1.f);
	const auto directions = direction_weights(input);

	std::uint32_t count = 0;
	const auto push = [&](const resource::handle<clip_asset>& clip, const float weight) {
		if (!clip.valid() || weight <= 0.0001f || count >= out.size()) {
			return;
		}
		out[count] = {
			.clip = clip,
			.weight = weight,
		};
		++count;
	};

	push(layout.idle, 1.f - throttle);

	for (std::size_t i = 0; i < locomotion_blend::direction_count; ++i) {
		const float share = directions[i] * throttle;
		if (share <= 0.0001f) {
			continue;
		}
		push(layout.walk[i], share * (1.f - run_share));
		push(layout.run[i].valid() ? layout.run[i] : layout.walk[i], share * run_share);
	}

	float total = 0.f;
	for (std::uint32_t i = 0; i < count; ++i) {
		total += out[i].weight;
	}
	if (total > 0.f) {
		for (std::uint32_t i = 0; i < count; ++i) {
			out[i].weight /= total;
		}
	}
	else if (count > 0) {
		out[0].weight = 1.f;
		count = 1;
	}

	return count;
}
