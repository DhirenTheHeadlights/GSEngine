export module gse.utility:system_clock;

import std;

import gse.physics.math;

import :clock;

export namespace gse::system_clock {
	auto update(
	) -> void;

	auto dt(
	) -> time;

	auto now(
	) -> time;

	auto fps(
	) -> std::uint32_t;

	constexpr time const_update_time = seconds(1.f / 100.f);
}

namespace gse::system_clock {
	clock main_clock;
	clock dt_clock;

	static constinit time delta_time;
	static constinit time frame_rate_update_time;

	static constinit std::uint32_t frame_count = 0;
	static constinit std::uint32_t frame_rate_count = 0;

	constexpr time fps_report_interval = seconds(1.0f);
}

auto gse::system_clock::update() -> void {
	delta_time = dt_clock.reset();

	frame_count++;
	frame_rate_update_time += delta_time;

	if (frame_rate_update_time >= fps_report_interval) {
		frame_rate_count = static_cast<std::uint32_t>(frame_count / frame_rate_update_time.as<seconds>());
		frame_count = 0;
		frame_rate_update_time -= fps_report_interval;
	}
}

auto gse::system_clock::dt() -> time {
	return delta_time > const_update_time ? delta_time : const_update_time;
}

auto gse::system_clock::now() -> time {
	return main_clock.elapsed();
}

auto gse::system_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}
