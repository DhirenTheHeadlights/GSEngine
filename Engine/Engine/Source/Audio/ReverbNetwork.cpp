module gse.audio:reverb_network_impl;

import std;

import gse.math;

import :reverb_network;

gse::audio::reverb_network::reverb_network() : m_lines(static_cast<std::size_t>(reverb_line_count) * line_frames), m_early(early_frames) {}

auto gse::audio::next_prime(const std::uint32_t from) -> std::uint32_t {
	const auto prime = [](const std::uint32_t candidate) {
		for (std::uint32_t divisor = 2; divisor * divisor <= candidate; ++divisor) {
			if (candidate % divisor == 0) {
				return false;
			}
		}
		return candidate > 1;
	};

	std::uint32_t candidate = from;
	while (!prime(candidate)) {
		++candidate;
	}
	return candidate;
}

auto gse::audio::reverb_network::tune(const reverb_setting& setting, const frequency rate, const std::uint32_t ramp_frames) -> void {
	constexpr float nearest_tap_frames = 1.f;
	constexpr float farthest_tap_frames = early_frames - 2.f;
	const time closest_spacing = milliseconds(5.f);
	const time widest_spacing = milliseconds(60.f);

	m_spacing_frames = static_cast<float>(std::clamp(setting.spacing, closest_spacing, widest_spacing) * rate);
	if (setting.decay > time{}) {
		m_decay_frames = static_cast<float>(setting.decay * rate);
	}
	m_send = setting.send;
	for (const auto [early, heard] : std::views::zip(m_target.early, setting.early)) {
		early = {
			.delay_frames = std::clamp(static_cast<float>(heard.delay * rate), nearest_tap_frames, farthest_tap_frames),
			.left = heard.left,
			.right = heard.right,
		};
	}
	if (!m_awake) {
		return;
	}

	m_target.feedback = feedback_for(m_decay_frames);
	m_target.send = send_for(m_target.feedback);
	m_ramp_left = ramp_frames;
	if (ramp_frames == 0) {
		m_now = m_target;
	}
	else {
		m_step = ramp_step(m_now, m_target, ramp_frames);
	}
}

auto gse::audio::reverb_network::feedback_for(const float decay_frames) const -> std::array<float, reverb_line_count> {
	constexpr float sixty_decibels = 3.f;

	std::array<float, reverb_line_count> feedback{};
	if (decay_frames <= 0.f) {
		return feedback;
	}
	for (const auto [kept, length] : std::views::zip(feedback, m_lengths)) {
		kept = std::pow(10.f, -sixty_decibels * static_cast<float>(length) / decay_frames);
	}
	return feedback;
}

auto gse::audio::reverb_network::send_for(const std::array<float, reverb_line_count>& feedback) const -> float {
	const float retained = std::ranges::fold_left(feedback, 0.f, [](const float sum, const float kept) {
		return sum + kept * kept;
	}) / static_cast<float>(reverb_line_count);
	return m_send * std::sqrt(1.f - retained);
}

auto gse::audio::reverb_network::ramp_step(const levels& from, const levels& to, const std::uint32_t frames) -> levels {
	const auto span = static_cast<float>(frames);

	levels step{
		.send = (to.send - from.send) / span,
	};
	for (const auto [stride, begin, end] : std::views::zip(step.feedback, from.feedback, to.feedback)) {
		stride = (end - begin) / span;
	}
	for (const auto [stride, begin, end] : std::views::zip(step.early, from.early, to.early)) {
		stride = {
			.delay_frames = (end.delay_frames - begin.delay_frames) / span,
			.left = (end.left - begin.left) / span,
			.right = (end.right - begin.right) / span,
		};
	}
	return step;
}

auto gse::audio::reverb_network::advance(levels& now, const levels& step) -> void {
	now.send += step.send;
	for (const auto [kept, stride] : std::views::zip(now.feedback, step.feedback)) {
		kept += stride;
	}
	for (const auto [early, stride] : std::views::zip(now.early, step.early)) {
		early.delay_frames += stride.delay_frames;
		early.left += stride.left;
		early.right += stride.right;
	}
}

auto gse::audio::reverb_network::wake() -> void {
	constexpr std::array<float, reverb_line_count> spread{ 1.f, 1.13f, 1.27f, 1.42f, 1.59f, 1.78f, 2.03f, 2.3f };

	for (const auto [length, ratio] : std::views::zip(m_lengths, spread)) {
		length = std::min(next_prime(static_cast<std::uint32_t>(std::lround(m_spacing_frames * ratio))), line_frames - 1);
	}
	m_cursors = {};
	m_target.feedback = feedback_for(m_decay_frames);
	m_target.send = send_for(m_target.feedback);
	m_now = m_target;
	m_ramp_left = 0;
	m_quiet_frames = 0;
	m_awake = true;
}

auto gse::audio::reverb_network::rest() -> void {
	std::ranges::fill(m_lines, 0.f);
	std::ranges::fill(m_early, 0.f);
	m_awake = false;
}

auto gse::audio::reverb_network::reflected(const tap& early) const -> float {
	const float whole = std::floor(early.delay_frames);
	const auto age = static_cast<std::uint32_t>(whole);
	const float newer = m_early[(m_early_head - age) % early_frames];
	const float older = m_early[(m_early_head - age - 1) % early_frames];
	return newer + (older - newer) * (early.delay_frames - whole);
}

auto gse::audio::reverb_network::process(const std::span<const float> feed, const std::span<float> out) -> void {
	constexpr float inaudible = 1e-7f;
	constexpr float vanishing = 1e-18f;
	constexpr float householder = 2.f / static_cast<float>(reverb_line_count);
	const float line_gain = 1.f / std::sqrt(static_cast<float>(reverb_line_count));

	if (!m_awake) {
		const bool fed = std::ranges::any_of(feed, [](const float sample) {
			return sample != 0.f;
		});
		if (m_send <= 0.f || !fed) {
			return;
		}
		wake();
	}

	float loudest = 0.f;
	for (const auto [dry, frame] : std::views::zip(feed, out | std::views::chunk(output_channels))) {
		if (m_ramp_left > 0) {
			--m_ramp_left;
			if (m_ramp_left == 0) {
				m_now = m_target;
			}
			else {
				advance(m_now, m_step);
			}
		}

		m_early[m_early_head] = dry;
		std::array<float, output_channels> wet{};
		for (const auto& early : m_now.early) {
			const float bounced = reflected(early);
			wet[0] += bounced * early.left;
			wet[1] += bounced * early.right;
		}
		m_early_head = (m_early_head + 1) % early_frames;

		std::array<float, reverb_line_count> returned;
		float circulating = 0.f;
		for (std::uint32_t line = 0; line < reverb_line_count; ++line) {
			const float held = m_lines[static_cast<std::size_t>(line) * line_frames + m_cursors[line]];
			wet[line % output_channels] += held * line_gain;
			returned[line] = std::abs(held) < vanishing ? 0.f : held * m_now.feedback[line];
			circulating += returned[line];
		}
		const float injected = dry * m_now.send - circulating * householder;
		for (std::uint32_t line = 0; line < reverb_line_count; ++line) {
			m_lines[static_cast<std::size_t>(line) * line_frames + m_cursors[line]] = returned[line] + injected;
			m_cursors[line] = m_cursors[line] + 1 == m_lengths[line] ? 0 : m_cursors[line] + 1;
		}

		frame[0] += wet[0];
		frame[1] += wet[1];
		loudest = std::max({ loudest, std::abs(wet[0]), std::abs(wet[1]) });
	}

	m_quiet_frames = loudest < inaudible ? m_quiet_frames + static_cast<std::uint32_t>(feed.size()) : 0;
	if (m_quiet_frames > line_frames + early_frames) {
		rest();
	}
}
