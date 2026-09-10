module gse.runtime:frame_pacing_impl;

import std;

import :frame_pacing;

import gse.core;
import gse.math;
import gse.time;
import gse.log;
import gse.win32;

namespace gse {
	constexpr double dedicated_polls_per_step = 2.0;
}

auto gse::pace_wait(frame_pacing& pacing, const time_t<double, seconds> deadline) -> void {
	if (!pacing.timer_created) {
		pacing.timer_created = true;
		pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, win32::create_waitable_timer_high_resolution, win32::timer_all_access);
		if (!pacing.timer) {
			pacing.timer = win32::CreateWaitableTimerExW(nullptr, nullptr, 0, win32::timer_all_access);
		}
	}

	const time_t<double, seconds> spin_margin = microseconds(400.0);
	const auto begin = system_clock::now<time_t<double, seconds>>();
	if (pacing.timer && deadline - begin > spin_margin) {
		const auto wait = deadline - begin - spin_margin;
		const win32::LARGE_INTEGER due{ .QuadPart = -static_cast<win32::LONGLONG>(wait / nanoseconds(100.0)) };
		if (win32::SetWaitableTimer(pacing.timer, &due, 0, nullptr, nullptr, 0)) {
			win32::WaitForSingleObject(pacing.timer, win32::infinite);
		}
	}
	while (system_clock::now<time_t<double, seconds>>() < deadline) {
		std::this_thread::yield();
	}
}

auto gse::pace_dedicated(frame_pacing& pacing) -> void {
	const auto period = system_clock::fixed_dt<time_t<double, seconds>>() / dedicated_polls_per_step;
	const auto now = system_clock::now<time_t<double, seconds>>();

	if (!pacing.deadline_valid) {
		pacing.deadline_valid = true;
		pacing.next_deadline = now + period;
		log::println(log::category::general, "dedicated pacing: engaged at {:.3f:ms} per loop ({} polls per fixed step)", period, dedicated_polls_per_step);
		return;
	}

	if (pacing.next_deadline > now) {
		pace_wait(pacing, pacing.next_deadline);
	}

	pacing.next_deadline += period;
	if (const auto after = system_clock::now<time_t<double, seconds>>(); pacing.next_deadline < after) {
		pacing.next_deadline = after + period;
	}
}
