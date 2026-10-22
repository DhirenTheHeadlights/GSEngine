module gse.runtime:frame_pacing_platform;

import std;

import :frame_pacing;

import gse.core;
import gse.math;
import gse.time;

auto gse::pace_sleep_until(frame_pacing& pacing, const time_t<double, seconds> target) -> void {
	const auto wait = target - system_clock::now<time_t<double, seconds>>();
	if (wait <= time_t<double, seconds>{}) {
		return;
	}
	std::this_thread::sleep_for(std::chrono::duration<double>(wait.as<seconds>()));
}

auto gse::pace_shutdown(frame_pacing& pacing) -> void {}
