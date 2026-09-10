module gse.runtime:frame_pacing;

import gse.math;
import gse.win32;

namespace gse {
	struct frame_pacing {
		win32::HANDLE timer = nullptr;
		bool timer_created = false;
		time_t<double, seconds> next_deadline{};
		bool deadline_valid = false;
	};

	auto pace_wait(
		frame_pacing& pacing,
		time_t<double, seconds> deadline
	) -> void;

	auto pace_dedicated(
		frame_pacing& pacing
	) -> void;
}