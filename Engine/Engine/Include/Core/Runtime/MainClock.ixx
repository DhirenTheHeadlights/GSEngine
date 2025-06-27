export module gse.runtime:main_clock;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse::main_clock {
	auto update() -> void;
	auto get_raw_delta_time() -> time;
	auto get_current_time() -> time;
	auto get_constant_update_time() -> time;
	auto get_frame_rate() -> int;
}

std::chrono::steady_clock::time_point g_last_update = std::chrono::steady_clock::now();
gse::time g_dt;

int g_frame_rate = 0;
float g_frame_count = 0;
float g_num_frames_to_average = 40;
gse::time g_frame_rate_update_time;

gse::clock g_main_clock;

auto gse::main_clock::update() -> void {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> delta_time = now - g_last_update;
	g_last_update = now;

	// Update delta time (clamped to a max of 0.16 seconds to avoid big time jumps)
	g_dt = seconds(std::min(delta_time.count(), 0.16f));

	++g_frame_count;
	g_frame_rate_update_time += g_dt;

	if (g_frame_count >= g_num_frames_to_average) {
		g_frame_rate = static_cast<int>(g_frame_count / g_frame_rate_update_time.as<units::seconds>());
		g_frame_rate_update_time = time();
		g_frame_count = 0.f;
	}
}

auto gse::main_clock::get_raw_delta_time() -> time {
	return g_dt;
}

auto gse::main_clock::get_current_time() -> time {
	return g_main_clock.elapsed();
}

auto gse::main_clock::get_constant_update_time() -> time {
	return seconds(0.01f);
}

auto gse::main_clock::get_frame_rate() -> int {
	return g_frame_rate;
}