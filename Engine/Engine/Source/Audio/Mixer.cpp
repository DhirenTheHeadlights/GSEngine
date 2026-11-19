module gse.audio:mixer_impl;

import std;

import gse.concurrency;
import gse.core;
import gse.math;

import :mixer;
import :reverb_network;

namespace gse::audio {
	auto strike_pulse_frames() -> std::uint32_t;
}

auto gse::audio::sample_rate() -> frequency {
	return hertz(static_cast<float>(output_frame_rate));
}

gse::audio::mixer::mixer() : m_paths(voice_capacity), m_banks(voice_capacity), m_delay_lines(static_cast<std::size_t>(voice_capacity) * delay_line_frames), m_reverb_feed(reverb_feed_frames) {}

auto gse::audio::mixer::submit(const voice_command& command) -> bool {
	return m_commands.push(command);
}

auto gse::audio::mixer::submit(const tune_reverb& command) -> bool {
	return m_reverb_commands.push(command);
}

auto gse::audio::mixer::take_finished(std::uint32_t& slot) -> bool {
	return m_finished.pop(slot);
}

auto gse::audio::mixer::render(const std::span<float> out) -> void {
	tune_reverb tuned;
	while (m_reverb_commands.pop(tuned)) {
		m_reverb.tune(tuned.setting, sample_rate(), tuned.ramp_frames);
	}

	voice_command command;
	while (m_commands.pop(command)) {
		std::visit(
			[this](const auto& started_or_stopped) {
				apply(started_or_stopped);
			},
			command
		);
	}

	std::ranges::fill(out, 0.f);

	for (std::size_t mixed = 0; mixed < out.size(); mixed += reverb_feed_frames * output_channels) {
		const std::span piece = out.subspan(mixed, std::min(reverb_feed_frames * output_channels, out.size() - mixed));
		const std::span reverb_feed = std::span(m_reverb_feed).first(piece.size() / output_channels);
		std::ranges::fill(reverb_feed, 0.f);

		for (std::uint32_t slot = 0; slot < voice_capacity; ++slot) {
			auto& [source, gain, placed] = m_voices[slot];
			bool ended = false;
			if (placed) {
				ended = mix_placed(slot, piece, reverb_feed);
			}
			else if (auto* cursor = std::get_if<clip_cursor>(&source)) {
				ended = mix(*cursor, gain, piece);
			}
			else if (auto* tone = std::get_if<oscillator>(&source)) {
				ended = mix(*tone, gain, piece);
			}
			if (ended) {
				finish(slot);
			}
		}

		m_reverb.process(reverb_feed, piece);
	}
}

auto gse::audio::mixer::apply(const start_clip& command) -> void {
	m_voices[command.slot] = {
		.source = clip_cursor{ .samples = command.samples, .loop = command.loop },
		.gain = command.gain.value(),
		.placed = place_voice(command.slot, command.at),
	};
}

auto gse::audio::mixer::apply(const start_tone& command) -> void {
	const angle half_step = radians(std::numbers::pi_v<float>) * (command.pitch / sample_rate());
	m_voices[command.slot] = {
		.source = oscillator{ .coefficient = 2.f * sin(half_step), .cosine = cos(half_step) },
		.gain = command.gain.value(),
		.placed = place_voice(command.slot, command.at),
	};
}

auto gse::audio::mixer::apply(const move_voice& command) -> void {
	if (!m_voices[command.slot].placed) {
		return;
	}
	auto& path = m_paths[command.slot];
	path.target = arrival_of(command.at);
	path.shadow_feed = shadow_feed_of(command.at);
	path.ramp_frames = command.ramp_frames;
	if (command.ramp_frames == 0) {
		path.now = path.target;
		return;
	}
	path.step = {
		.left = ramp_step(path.now.left, path.target.left, command.ramp_frames),
		.right = ramp_step(path.now.right, path.target.right, command.ramp_frames),
		.damping = (path.target.damping - path.now.damping) / static_cast<float>(command.ramp_frames),
		.send = (path.target.send - path.now.send) / static_cast<float>(command.ramp_frames),
	};
}

auto gse::audio::mixer::ramp_step(const ear_arrival& from, const ear_arrival& to, const std::uint32_t frames) -> ear_arrival {
	const auto span = static_cast<float>(frames);
	return {
		.delay_frames = (to.delay_frames - from.delay_frames) / frames,
		.gain = (to.gain - from.gain) / span,
		.shadow = (to.shadow - from.shadow) / span,
	};
}

auto gse::audio::mixer::arrival_of(const ear& at) -> ear_arrival {
	constexpr double nearest_frames = 2.0;
	constexpr double farthest_frames = delay_line_frames - 4.0;

	return {
		.delay_frames = std::clamp(static_cast<double>(at.delay * sample_rate()), nearest_frames, farthest_frames),
		.gain = at.gain.value(),
		.shadow = at.shadow,
	};
}

auto gse::audio::mixer::arrival_of(const placement& at) -> arrival {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;

	return {
		.left = arrival_of(at.left),
		.right = arrival_of(at.right),
		.damping = 1.f - std::exp(-turn * (at.absorption_cutoff / sample_rate())),
		.send = at.send,
	};
}

auto gse::audio::mixer::shadow_feed_of(const placement& at) -> float {
	const angle half_step = radians(std::numbers::pi_v<float>) * (at.shadow_corner / sample_rate());
	return 1.f / (1.f + tan(half_step));
}

auto gse::audio::mixer::place_voice(const std::uint32_t slot, const std::optional<placement>& at) -> bool {
	if (!at) {
		return false;
	}
	const arrival start = arrival_of(*at);
	m_paths[slot] = {
		.now = start,
		.target = start,
		.shadow_feed = shadow_feed_of(*at),
	};
	return true;
}

auto gse::audio::mixer::apply(const stop_voice& command) -> void {
	if (std::holds_alternative<std::monostate>(m_voices[command.slot].source)) {
		return;
	}
	finish(command.slot);
}

auto gse::audio::mixer::finish(const std::uint32_t slot) -> void {
	m_voices[slot] = {};
	m_finished.push(slot);
}

auto gse::audio::mixer::mix_placed(const std::uint32_t slot, const std::span<float> out, const std::span<float> reverb_feed) -> bool {
	constexpr std::uint32_t interpolation_reach = 3;

	auto& source = m_voices[slot].source;
	const float gain = m_voices[slot].gain;
	auto& path = m_paths[slot];
	const std::span line = std::span(m_delay_lines).subspan(static_cast<std::size_t>(slot) * delay_line_frames, delay_line_frames);
	std::array<float, chunk_frames> scratch;
	std::size_t fed = 0;

	for (const auto block : out | std::views::chunk(chunk_frames * output_channels)) {
		const std::span dry = std::span(scratch).first(block.size() / output_channels);
		std::size_t produced = 0;
		if (auto* cursor = std::get_if<clip_cursor>(&source)) {
			produced = fill(*cursor, gain, dry);
		}
		else if (auto* tone = std::get_if<oscillator>(&source)) {
			produced = fill(*tone, gain, dry);
		}
		else if (std::holds_alternative<ringing>(source)) {
			produced = fill(m_banks[slot], dry);
		}
		std::ranges::fill(dry.subspan(produced), 0.f);
		path.silent_frames = produced == dry.size() ? 0 : path.silent_frames + static_cast<std::uint32_t>(dry.size() - produced);

		for (const auto [sample, frame] : std::views::zip(dry, block | std::views::chunk(output_channels))) {
			line[path.head] = sample;
			path.written = std::min(path.written + 1, delay_line_frames);
			advance(path);
			reverb_feed[fed++] += sample * path.now.send;
			frame[0] += heard(line, path, path.now.left, path.left);
			frame[1] += heard(line, path, path.now.right, path.right);
			path.head = (path.head + 1) % delay_line_frames;
		}
	}

	return path.silent_frames > std::max(path.now.left.delay_frames, path.now.right.delay_frames) + interpolation_reach;
}

auto gse::audio::mixer::heard(const std::span<const float> line, const propagation& path, const ear_arrival& at, ear_filter& filter) -> float {
	filter.low_passed += path.now.damping * (delayed(line, path, at.delay_frames) - filter.low_passed);
	filter.shadow_high = path.shadow_feed * (filter.low_passed - filter.shadow_input) + (2.f * path.shadow_feed - 1.f) * filter.shadow_high;
	filter.shadow_input = filter.low_passed;
	return (filter.low_passed + (at.shadow - 1.f) * filter.shadow_high) * at.gain;
}

auto gse::audio::mixer::advance(propagation& path) -> void {
	if (path.ramp_frames == 0) {
		return;
	}
	--path.ramp_frames;
	if (path.ramp_frames == 0) {
		path.now = path.target;
		return;
	}
	advance(path.now.left, path.step.left);
	advance(path.now.right, path.step.right);
	path.now.damping += path.step.damping;
	path.now.send += path.step.send;
}

auto gse::audio::mixer::advance(ear_arrival& now, const ear_arrival& step) -> void {
	now.delay_frames += step.delay_frames;
	now.gain += step.gain;
	now.shadow += step.shadow;
}

auto gse::audio::mixer::delayed(const std::span<const float> line, const propagation& path, const double delay_frames) -> float {
	const double whole = std::floor(delay_frames);
	const auto newer_age = static_cast<std::uint32_t>(whole);
	const auto t = static_cast<float>(1.0 - (delay_frames - whole));

	const float x0 = tap(line, path, newer_age + 2);
	const float x1 = tap(line, path, newer_age + 1);
	const float x2 = tap(line, path, newer_age);
	const float x3 = tap(line, path, newer_age - 1);

	const float c1 = 0.5f * (x2 - x0);
	const float c2 = x0 - 2.5f * x1 + 2.f * x2 - 0.5f * x3;
	const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
	return ((c3 * t + c2) * t + c1) * t + x1;
}

auto gse::audio::mixer::tap(const std::span<const float> line, const propagation& path, const std::uint32_t age) -> float {
	return age < path.written ? line[(path.head - age) % delay_line_frames] : 0.f;
}

auto gse::audio::mixer::fill(clip_cursor& cursor, const float gain, const std::span<float> dry) -> std::size_t {
	const float mono_gain = gain / static_cast<float>(output_channels);
	for (std::size_t frame = 0; frame < dry.size(); ++frame) {
		if (cursor.next == cursor.samples.size()) {
			if (!cursor.loop || cursor.samples.empty()) {
				return frame;
			}
			cursor.next = 0;
		}
		const auto channels = cursor.samples.subspan(cursor.next, output_channels);
		dry[frame] = std::ranges::fold_left(channels, 0.f, std::plus{}) * mono_gain;
		cursor.next += output_channels;
	}
	return dry.size();
}

auto gse::audio::mixer::fill(oscillator& tone, const float gain, const std::span<float> dry) -> std::size_t {
	for (float& sample : dry) {
		sample = next(tone) * gain;
	}
	return dry.size();
}

auto gse::audio::mixer::tune(bank& struck, const std::span<const resonance> rings, const inverse_time least_decay) -> void {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;

	struck = {
		.count = static_cast<std::uint32_t>(std::min<std::size_t>(rings.size(), resonance_capacity)),
	};
	for (const auto [mode, ring] : std::views::zip(struck.modes, rings)) {
		const float kept = std::exp(-(std::max(ring.decay, least_decay) / sample_rate()));
		const angle step = radians(turn) * (ring.pitch / sample_rate());
		mode.turn_real = kept * cos(step);
		mode.turn_imag = kept * sin(step);
	}
}

auto gse::audio::mixer::apply(const start_ringing& command) -> void {
	tune(m_banks[command.slot], command.rings, inverse_time{});
	m_voices[command.slot] = {
		.source = ringing{},
		.gain = 1.f,
		.placed = place_voice(command.slot, command.at),
	};
	apply(strike_voice{ .slot = command.slot, .drive = command.drive });
}

auto gse::audio::mixer::apply(const start_rubbing& command) -> void {
	constexpr std::uint32_t golden_stride = 0x9e3779b9u;

	auto& rubbed = m_banks[command.slot];
	tune(rubbed, command.rings, command.least_decay);
	rubbed.drive = command.drive;
	rubbed.rub = rubbing{
		.noise = golden_stride * (command.slot + 1),
	};
	m_voices[command.slot] = {
		.source = ringing{},
		.gain = 1.f,
		.placed = place_voice(command.slot, command.at),
	};
}

auto gse::audio::mixer::apply(const rub_voice& command) -> void {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;

	auto& rub = *m_banks[command.slot].rub;
	ramp(rub, command.level, command.ramp_frames);
	rub.feed = 1.f - std::exp(-turn * (command.cutoff / sample_rate()));
	rub.makeup = rub.feed > 0.f ? std::sqrt((2.f - rub.feed) / rub.feed) : 0.f;
	rub.wobble_step = command.wobble / sample_rate();
	rub.wobble_depth = command.wobble_depth;
}

auto gse::audio::mixer::apply(const fade_rub& command) -> void {
	ramp(*m_banks[command.slot].rub, 0.f, command.ramp_frames);
}

auto gse::audio::mixer::ramp(rubbing& rub, const float target, const std::uint32_t frames) -> void {
	rub.target = target;
	rub.ramp_left = frames;
	if (frames == 0) {
		rub.level = target;
	}
	else {
		rub.level_step = (target - rub.level) / static_cast<float>(frames);
	}
}

auto gse::audio::strike_pulse_frames() -> std::uint32_t {
	const time pulse = microseconds(250.f);
	return static_cast<std::uint32_t>(std::lround(sample_rate() * pulse));
}

auto gse::audio::mixer::apply(const strike_voice& command) -> void {
	auto& struck = m_banks[command.slot];
	struck.drive = command.drive;
	struck.pulse_left = strike_pulse_frames();
}

auto gse::audio::mixer::fill(bank& struck, const std::span<float> dry) -> std::size_t {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;

	if (struck.rub) {
		fill(struck, *struck.rub, dry);
		return dry.size();
	}

	const auto pulse_frames = static_cast<float>(strike_pulse_frames());
	const std::span modes = std::span(struck.modes).first(struck.count);

	if (struck.pulse_left == 0) {
		settle(modes);
	}

	for (float& sample : dry) {
		float push = 0.f;
		if (struck.pulse_left > 0) {
			const float along = (pulse_frames - static_cast<float>(struck.pulse_left) + 0.5f) / pulse_frames;
			push = (1.f - std::cos(turn * along)) / pulse_frames;
			--struck.pulse_left;
		}
		sample = ring(modes, struck.drive, push);
	}
	return dry.size();
}

auto gse::audio::mixer::fill(bank& rubbed, rubbing& rub, const std::span<float> dry) -> void {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;
	constexpr float noise_scale = 2.f / 4294967296.f;

	const std::span modes = std::span(rubbed.modes).first(rubbed.count);

	if (rub.ramp_left == 0 && rub.level == 0.f) {
		settle(modes);
	}

	for (float& sample : dry) {
		if (rub.ramp_left > 0) {
			--rub.ramp_left;
			rub.level = rub.ramp_left == 0 ? rub.target : rub.level + rub.level_step;
		}

		rub.noise ^= rub.noise << 13;
		rub.noise ^= rub.noise >> 17;
		rub.noise ^= rub.noise << 5;
		const float white = static_cast<float>(rub.noise) * noise_scale - 1.f;
		rub.low_passed += rub.feed * (white - rub.low_passed);

		rub.wobble_turns += rub.wobble_step;
		rub.wobble_turns -= std::floor(rub.wobble_turns);
		const float push = rub.low_passed * rub.makeup * rub.level * (1.f + rub.wobble_depth * std::sin(turn * rub.wobble_turns));

		sample = modes.empty() ? push : ring(modes, rubbed.drive, push);
	}
}

auto gse::audio::mixer::ring(const std::span<resonator> modes, const strike_drive& drive, const float push) -> float {
	float sum = 0.f;
	for (const auto [mode, driven] : std::views::zip(modes, drive)) {
		const float real = mode.turn_real * mode.real - mode.turn_imag * mode.imag + push * driven;
		mode.imag = mode.turn_imag * mode.real + mode.turn_real * mode.imag;
		mode.real = real;
		sum += mode.imag;
	}
	return sum;
}

auto gse::audio::mixer::settle(const std::span<resonator> modes) -> void {
	constexpr float inaudible_energy = 1e-16f;

	for (auto& mode : modes) {
		if (mode.real * mode.real + mode.imag * mode.imag < inaudible_energy) {
			mode.real = 0.f;
			mode.imag = 0.f;
		}
	}
}

auto gse::audio::mixer::next(oscillator& tone) -> float {
	const float value = tone.sine;
	tone.cosine -= tone.coefficient * tone.sine;
	tone.sine += tone.coefficient * tone.cosine;
	return value;
}

auto gse::audio::mixer::mix(clip_cursor& cursor, const float gain, const std::span<float> out) -> bool {
	for (float& sample : out) {
		if (cursor.next == cursor.samples.size()) {
			if (!cursor.loop || cursor.samples.empty()) {
				return true;
			}
			cursor.next = 0;
		}
		sample += cursor.samples[cursor.next++] * gain;
	}
	return false;
}

auto gse::audio::mixer::mix(oscillator& tone, const float gain, const std::span<float> out) -> bool {
	for (const auto frame : out | std::views::chunk(output_channels)) {
		const float value = next(tone) * gain;
		for (float& sample : frame) {
			sample += value;
		}
	}
	return false;
}
