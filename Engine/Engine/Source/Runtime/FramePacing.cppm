module gse.runtime:frame_pacing;

import std;

import gse.core;
import gse.math;
import gse.time;
import gse.win32;

namespace gse {
	struct frame_pacing {
		win32::HANDLE timer = nullptr;
		bool timer_created = false;
		time_t<std::uint64_t> refresh{};
		int divisor = 1;
		time_t<double, seconds> next_deadline{};
		bool deadline_valid = false;
		time_t<double, seconds> last_entry{};
		time_t<double, seconds> last_wait{};
		time_t<double, seconds> ema_busy{};
		time_t<double, seconds> next_heartbeat{};
		time_t<double, seconds> allow_down_at{};
		time_t<double, seconds> last_down_at{};
		time_t<double, seconds> down_backoff{};
		std::uint32_t window_frames = 0;
		std::uint32_t window_misses = 0;
		int pending_divisor = 1;
	};

	auto pace_wait(
		frame_pacing& pacing,
		time_t<double, seconds> deadline
	) -> void;

	auto pace_frame(
		frame_pacing& pacing
	) -> void;
}
