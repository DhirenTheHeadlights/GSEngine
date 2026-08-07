export module gse.time:system_clock;

import std;
import gse.math;
import gse.log;

import :clock;

export namespace gse::system_clock {
	using internal_time = time_t<>;
	using default_time = time_t<float, seconds>;

	auto update() -> void;

	auto submit_display_interval(
		internal_time dt
	) -> void;

	auto submit_refresh_interval(
		internal_time interval
	) -> void;

	template <is_quantity Q = default_time>
	auto dt() -> Q;

	template <is_quantity Q = default_time>
	auto now() -> Q;

	template <is_quantity Q = default_time>
	auto constant_update_time() -> Q;

	template <is_quantity Q = default_time>
	auto fixed_dt() -> Q;

	auto fixed_steps_this_frame() -> int;

	auto fixed_alpha() -> float;

	template <is_quantity Q = default_time>
	auto fixed_lag() -> Q;

	auto set_fixed_step_override(
		std::optional<int> steps
	) -> void;

	auto fps() -> std::uint32_t;

	auto timestamp_filename() -> std::string;
}

namespace gse::system_clock {
	constexpr internal_time fps_report_interval = seconds(1.0);
	constexpr internal_time const_update_time = milliseconds(16.6667);
	constexpr internal_time max_render_step = seconds(0.25);
	constexpr int max_fixed_steps = 4;
	constexpr int max_snap_multiple = 4;
	constexpr std::size_t delta_history_size = 11;

	clock main_clock;
	clock dt_clock;

	internal_time delta_time{};
	internal_time frame_rate_update_time{};
	internal_time fixed_accumulator{};
	internal_time refresh_interval{};
	internal_time snap_error{};
	std::optional<int> fixed_step_override;
	std::optional<internal_time> external_display_interval;

	std::array<internal_time, delta_history_size> delta_history{};
	std::size_t delta_history_count = 0;
	std::size_t delta_history_cursor = 0;

	std::uint32_t frame_count = 0;
	std::uint32_t frame_rate_count = 0;
	int fixed_steps_count = 0;

	auto update_frame_rate(
		internal_time elapsed
	) -> void;

	auto record_delta(
		internal_time delta
	) -> void;

	auto display_interval() -> internal_time;

	auto snap_delta(
		internal_time delta
	) -> internal_time;

	constexpr std::uint32_t pacing_health_check_frame = 600;

	auto report_pacing(
		internal_time wall_delta,
		bool external
	) -> void;

	std::uint32_t pacing_frames = 0;
	std::uint32_t pacing_external = 0;
	std::uint32_t pacing_raw = 0;
	bool pacing_reported = false;
}

auto gse::system_clock::update() -> void {
	if (fixed_step_override.has_value()) {
		fixed_steps_count = std::max(0, *fixed_step_override);
		delta_time = const_update_time * fixed_steps_count;
		fixed_accumulator = internal_time{};
		update_frame_rate(delta_time);
		return;
	}

	const auto wall_delta = gse::quantity_cast<internal_time>(dt_clock.reset<double>());
	record_delta(wall_delta);

	const bool external = external_display_interval.has_value();
	if (external) {
		delta_time = *external_display_interval;
		external_display_interval.reset();
		snap_error = internal_time{};
	}
	else {
		delta_time = snap_delta(wall_delta);
	}
	report_pacing(wall_delta, external);
	update_frame_rate(wall_delta);

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

auto gse::system_clock::report_pacing(const internal_time wall_delta, const bool external) -> void {
	++pacing_frames;
	if (external) {
		++pacing_external;
	}
	else if (delta_time == wall_delta) {
		++pacing_raw;
	}

	if (pacing_reported || pacing_frames < pacing_health_check_frame) {
		return;
	}
	pacing_reported = true;

	if (pacing_external * 2 < pacing_frames) {
		log::println(
			log::level::warning,
			log::category::general,
			"frame pacing degraded: {} of {} frames used display timing, {} passed raw CPU deltas through unsnapped (refresh={:.3f:ms}). "
			"CPU loop time does not match display cadence, so world motion will judder",
			pacing_external,
			pacing_frames,
			pacing_raw,
			refresh_interval
		);
	}
}

auto gse::system_clock::record_delta(const internal_time delta) -> void {
	delta_history[delta_history_cursor] = delta;
	delta_history_cursor = (delta_history_cursor + 1) % delta_history_size;
	if (delta_history_count < delta_history_size) {
		++delta_history_count;
	}
}

auto gse::system_clock::display_interval() -> internal_time {
	if (refresh_interval > internal_time{}) {
		return refresh_interval;
	}
	if (delta_history_count < delta_history_size) {
		return internal_time{};
	}
	auto sorted = delta_history;
	const auto middle = sorted.begin() + static_cast<std::ptrdiff_t>(delta_history_size / 2);
	std::ranges::nth_element(sorted, middle);
	return *middle;
}

auto gse::system_clock::snap_delta(const internal_time delta) -> internal_time {
	const internal_time snap_tolerance = milliseconds(1.5);
	const internal_time snap_error_limit = milliseconds(8.0);

	const auto interval = display_interval();
	if (interval <= internal_time{}) {
		return delta;
	}

	for (int multiple = 1; multiple <= max_snap_multiple; ++multiple) {
		const auto candidate = interval * multiple;
		if (abs(delta - candidate) > snap_tolerance) {
			continue;
		}
		if (const auto projected = snap_error + (delta - candidate); abs(projected) <= snap_error_limit) {
			snap_error = projected;
			return candidate;
		}
		break;
	}

	snap_error = internal_time{};
	return delta;
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
	return gse::quantity_cast<Q>(std::min(delta_time, max_render_step));
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

auto gse::system_clock::fixed_alpha() -> float {
	return std::clamp(fixed_accumulator / const_update_time, 0.f, 1.f);
}

template <gse::is_quantity Q>
auto gse::system_clock::fixed_lag() -> Q {
	return gse::quantity_cast<Q>(const_update_time * (1.f - fixed_alpha()));
}

auto gse::system_clock::set_fixed_step_override(const std::optional<int> steps) -> void {
	fixed_step_override = steps;
}

auto gse::system_clock::submit_display_interval(const internal_time dt) -> void {
	external_display_interval = dt;
}

auto gse::system_clock::submit_refresh_interval(const internal_time interval) -> void {
	refresh_interval = interval;
}

auto gse::system_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}

auto gse::system_clock::timestamp_filename() -> std::string {
	const auto now = std::chrono::system_clock::now();
	return std::format("{:%Y%m%d_%H%M%S}", std::chrono::floor<std::chrono::seconds>(now));
}
