export module gse.utility:system_clock;

import std;

import gse.physics.math;

import :clock;

export namespace gse::system_clock {
	auto update(
	) -> void;

	template <typename T = float>
	auto dt(
	) -> time_t<T>;

	template <typename T = float>
	auto now(
	) -> time_t<T>;

	auto fps(
	) -> std::uint32_t;

	template <typename T = float>
	auto constant_update_time(
	) -> time_t<T>;
}

namespace gse::system_clock {
	clock main_clock;
	clock dt_clock;

	time_t<double> delta_time;
    time_t<double> frame_rate_update_time;

	std::uint32_t frame_count = 0;
	std::uint32_t frame_rate_count = 0;

	constexpr time_t<double> fps_report_interval = seconds(1.0);
	constexpr time_t<double> const_update_time = milliseconds(10.0);
}

auto gse::system_clock::update() -> void {
	delta_time = dt_clock.reset<double>();

	frame_count++;
	frame_rate_update_time += delta_time;

	if (frame_rate_update_time >= fps_report_interval) {
		frame_rate_count = static_cast<std::uint32_t>(frame_count / frame_rate_update_time.as<seconds>());
		frame_count = 0;
		frame_rate_update_time -= fps_report_interval;
	}
}

template <typename T>
auto gse::system_clock::dt() -> time_t<T> {
	return quantity_cast<time_t<T>>(std::min(delta_time, const_update_time));
}

template <typename T>
auto gse::system_clock::now() -> time_t<T> {
	return main_clock.elapsed<T>();
}

template <typename T>
auto gse::system_clock::constant_update_time() -> time_t<T> {
	return quantity_cast<time_t<T>>(const_update_time);
}

auto gse::system_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}