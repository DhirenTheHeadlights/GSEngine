module gse.runtime:frame_pacing;

import gse.math;

namespace gse {
	struct frame_pacing {
		void* timer = nullptr;
		bool timer_created = false;
		time_t<double, seconds> next_deadline{};
		bool deadline_valid = false;
	};

	auto pace_sleep_until(
		frame_pacing& pacing,
		time_t<double, seconds> target
	) -> void;

	auto pace_shutdown(
		frame_pacing& pacing
	) -> void;

	auto pace_wait(
		frame_pacing& pacing,
		time_t<double, seconds> deadline
	) -> void;

	auto pace_dedicated(
		frame_pacing& pacing
	) -> void;
}
