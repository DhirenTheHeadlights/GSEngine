module gse.runtime:frame_pacing_impl;

import std;

import :frame_pacing;

import gse.core;
import gse.math;
import gse.time;
import gse.log;
import gse.win32;

namespace gse {
	constexpr std::uint32_t pacing_window = 120;
	constexpr int pacing_max_divisor = 4;
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
	pacing.last_wait = system_clock::now<time_t<double, seconds>>() - begin;
}

auto gse::pace_frame(frame_pacing& pacing) -> void {
	if (pacing.refresh == time_t<std::uint64_t>{}) {
		return;
	}

	const auto refresh = quantity_cast<time_t<double, seconds>>(time(pacing.refresh));
	system_clock::submit_refresh_interval(quantity_cast<system_clock::internal_time>(refresh));

	const auto target = refresh * static_cast<double>(pacing.divisor);
	const auto now = system_clock::now<time_t<double, seconds>>();

	if (!pacing.deadline_valid) {
		pacing.deadline_valid = true;
		pacing.next_deadline = now + target;
		pacing.last_entry = now;
		pacing.last_wait = {};
		pacing.ema_busy = target;
		pacing.next_heartbeat = now + seconds(10.0);
		pacing.allow_down_at = now;
		pacing.last_down_at = {};
		pacing.down_backoff = seconds(5.0);
		log::println(log::category::general, "attached pacing: engaged at editor refresh {:.3f:ms}", refresh);
		return;
	}

	const double busy_alpha = 0.1;
	const auto busy = now - pacing.last_entry - pacing.last_wait;
	pacing.last_entry = now;
	pacing.last_wait = {};
	pacing.ema_busy = pacing.ema_busy * (1.0 - busy_alpha) + busy * busy_alpha;

	++pacing.window_frames;
	if (now > pacing.next_deadline) {
		++pacing.window_misses;
		pacing.next_deadline = now + target;
		const auto miss_delta = busy > pacing.ema_busy * 2.0 ? time_t<double, seconds>(busy) : pacing.ema_busy;
		system_clock::submit_display_interval(quantity_cast<system_clock::internal_time>(miss_delta));
	}
	else {
		pace_wait(pacing, pacing.next_deadline);
		pacing.next_deadline += target;
		system_clock::submit_display_interval(quantity_cast<system_clock::internal_time>(target));
	}

	if (pacing.window_frames >= pacing_window) {
		const double load = pacing.ema_busy * 1.1 / refresh;
		auto desired = std::clamp(static_cast<int>(std::ceil(load)), 1, pacing_max_divisor);
		if (pacing.window_misses * 4 > pacing.window_frames) {
			desired = std::clamp(std::max(desired, pacing.divisor + 1), 1, pacing_max_divisor);
		}
		if (desired != pacing.divisor) {
			if (desired != pacing.pending_divisor) {
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms} suggests every {} refresh(es), confirming next window", pacing.ema_busy, desired);
			}
			else if (desired > pacing.divisor) {
				pacing.divisor = desired;
				if (now - pacing.last_down_at < seconds(6.0)) {
					if (const auto doubled = time_t<double, seconds>(pacing.down_backoff * 2.0); doubled < seconds(60.0)) {
						pacing.down_backoff = doubled;
					}
					else {
						pacing.down_backoff = seconds(60.0);
					}
				}
				pacing.allow_down_at = now + pacing.down_backoff;
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms}, now pacing to every {} editor refresh(es) ({:.3f:ms} target)", pacing.ema_busy, desired, refresh * static_cast<double>(desired));
			}
			else if (now >= pacing.allow_down_at) {
				pacing.divisor = desired;
				pacing.last_down_at = now;
				log::println(log::category::general, "attached pacing: frame cost {:.3f:ms}, probing every {} editor refresh(es) ({:.3f:ms} target)", pacing.ema_busy, desired, refresh * static_cast<double>(desired));
			}
		}
		pacing.pending_divisor = desired;
		pacing.window_frames = 0;
		pacing.window_misses = 0;
	}

	if (now >= pacing.next_heartbeat) {
		pacing.next_heartbeat = now + seconds(30.0);
		log::println(log::category::general, "attached pacing: divisor={} frame cost {:.3f:ms} misses={}/{} in current window", pacing.divisor, pacing.ema_busy, pacing.window_misses, pacing.window_frames);
	}
}
