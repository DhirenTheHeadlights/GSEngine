export module gse.time:system_clock;

import std;
import gse.math;

import :clock;

export namespace gse::system_clock {
	using internal_time = time_t<>;
	using default_time = time_t<float, seconds>;

	auto update() -> void;

	template <is_quantity Q = default_time>
	auto dt() -> Q;

	template <is_quantity Q = default_time>
	auto now() -> Q;

	template <is_quantity Q = default_time>
	auto constant_update_time() -> Q;

	template <is_quantity Q = default_time>
	auto fixed_dt() -> Q;

	auto fixed_steps_this_frame() -> int;

	auto set_fixed_step_override(
		std::optional<int> steps
	) -> void;

	auto fps() -> std::uint32_t;

	auto timestamp_filename() -> std::string;
}

namespace gse::system_clock {
	clock main_clock;
	clock dt_clock;

	internal_time delta_time{};
	internal_time frame_rate_update_time{};
	internal_time fixed_accumulator{};
	std::optional<int> fixed_step_override;

	constexpr internal_time fps_report_interval = seconds(1.0);
	constexpr internal_time const_update_time = milliseconds(16.6667);
	constexpr internal_time max_render_step = seconds(0.25);
	constexpr int max_fixed_steps = 4;

	std::uint32_t frame_count = 0;
	std::uint32_t frame_rate_count = 0;
	int fixed_steps_count = 0;

	auto update_frame_rate(
		internal_time elapsed
	) -> void;
}

auto gse::system_clock::update() -> void {
	if (fixed_step_override.has_value()) {
		fixed_steps_count = std::max(0, *fixed_step_override);
		delta_time = const_update_time * fixed_steps_count;
		fixed_accumulator = internal_time{};
		update_frame_rate(delta_time);
		return;
	}

	delta_time = gse::quantity_cast<internal_time>(dt_clock.reset<double>());
	update_frame_rate(delta_time);

	fixed_accumulator += std::min(delta_time, max_render_step);
	fixed_steps_count = 0;
	while (fixed_accumulator >= const_update_time) {
		fixed_accumulator -= const_update_time;
		fixed_steps_count++;
	}
	if (fixed_steps_count > max_fixed_steps) {
		fixed_accumulator = internal_time{};
		fixed_steps_count = max_fixed_steps;
	}
}

auto gse::system_clock::update_frame_rate(const internal_time elapsed) -> void {
	frame_count++;
	frame_rate_update_time += elapsed;

	if (frame_rate_update_time >= fps_report_interval) {
		frame_rate_count = static_cast<std::uint32_t>(frame_count / frame_rate_update_time.as<seconds>());
		frame_count = 0;
		frame_rate_update_time -= fps_report_interval;
	}
}

template <gse::is_quantity Q>
auto gse::system_clock::dt() -> Q {
	return gse::quantity_cast<Q>(std::min(delta_time, const_update_time));
}

template <gse::is_quantity Q>
auto gse::system_clock::now() -> Q {
	return gse::quantity_cast<Q>(main_clock.elapsed<double>());
}

template <gse::is_quantity Q>
auto gse::system_clock::constant_update_time() -> Q {
	return gse::quantity_cast<Q>(const_update_time);
}

template <gse::is_quantity Q>
auto gse::system_clock::fixed_dt() -> Q {
	return gse::quantity_cast<Q>(const_update_time);
}

auto gse::system_clock::fixed_steps_this_frame() -> int {
	return fixed_steps_count;
}

auto gse::system_clock::set_fixed_step_override(const std::optional<int> steps) -> void {
	fixed_step_override = steps;
}

auto gse::system_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}

auto gse::system_clock::timestamp_filename() -> std::string {
	const auto now = std::chrono::system_clock::now();
	return std::format("{:%Y%m%d_%H%M%S}", std::chrono::floor<std::chrono::seconds>(now));
}
