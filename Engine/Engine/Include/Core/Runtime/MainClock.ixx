export module gse.runtime:main_clock;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse::main_clock {
	auto update() -> void;

	auto raw_dt() -> time;
	auto current_time() -> time;
	auto frame_rate() -> std::uint32_t;

	constexpr time const_update_time = seconds(1.f / 100.f);
}

namespace gse::main_clock {
	clock main_clock;
	clock dt_clock;
	static constinit time dt;
	static constinit time frame_rate_update_time;

	static constinit std::uint32_t frame_count = 0;
	static constinit std::uint32_t frame_rate_count = 0;
	constexpr std::uint32_t frame_rate_update_interval = 60; 
}

auto gse::main_clock::update() -> void {
	dt = std::min(dt, const_update_time);

	++frame_count;
	frame_rate_update_time += dt;

	if (frame_count >= frame_rate_update_interval) {
		frame_rate_count = static_cast<int>(frame_count / frame_rate_update_time.as<units::seconds>());
		frame_rate_update_time = {};
		frame_count = 0.f;
	}
}

auto gse::main_clock::raw_dt() -> time {
	return dt;
}

auto gse::main_clock::current_time() -> time {
	return main_clock.elapsed();
}

auto gse::main_clock::frame_rate() -> std::uint32_t {
	return frame_rate_count;
}