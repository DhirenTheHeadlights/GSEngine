module gse.runtime:frame_pacing_impl;

import std;

import :frame_pacing;

import gse.core;
import gse.math;
import gse.time;
import gse.log;

namespace gse {
	constexpr double dedicated_polls_per_step = 2.0;
}

auto gse::pace_wait(frame_pacing& pacing, const time_t<double, seconds> deadline) -> void {
	const time_t<double, seconds> spin_margin = microseconds(400.0);
	const auto begin = system_clock::now<time_t<double, seconds>>();
	if (deadline - begin > spin_margin) {
		pace_sleep_until(pacing, deadline - spin_margin);
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
