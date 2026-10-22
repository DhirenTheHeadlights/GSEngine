module gse.runtime:frame_pacing_platform;

import std;

import :frame_pacing;

import gse.core;
import gse.math;
import gse.time;
import gse.win32;

auto gse::pace_sleep_until(frame_pacing& pacing, const time_t<double, seconds> target) -> void {
	if (!pacing.timer_created) {
		pacing.timer_created = true;
		pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, win32::create_waitable_timer_high_resolution, win32::timer_all_access);
		if (!pacing.timer) {
			pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, 0, win32::timer_all_access);
		}
	}

	if (!pacing.timer) {
		return;
	}

	const auto wait = target - system_clock::now<time_t<double, seconds>>();
	if (wait <= time_t<double, seconds>{}) {
		return;
	}

	const win32::LARGE_INTEGER due{ .QuadPart = -static_cast<win32::LONGLONG>(wait / nanoseconds(100.0)) };
	if (win32::SetWaitableTimer(pacing.timer, &due, 0, nullptr, nullptr, 0)) {
		win32::WaitForSingleObject(pacing.timer, win32::infinite);
	}
}

auto gse::pace_shutdown(frame_pacing& pacing) -> void {
	if (pacing.timer) {
		win32::CloseHandle(pacing.timer);
		pacing.timer = nullptr;
	}
}
