export module gse.gpu:shared_output;

import std;

import gse.diag;
import gse.log;
import gse.math;
import gse.time;

import :frame_output;

export namespace gse::gpu {
	constexpr std::size_t shared_output_slots = 3;

	enum class shared_output_stage : std::uint8_t {
		off,
		requested [[
			= entered_from({ off })
		]],
		ready [[
			= entered_from({ requested })
		]],
		abandoned [[
			= entered_from({ requested })
		]],
	};

	struct shared_output_state {
		lifecycle<shared_output_stage> stage{ "shared_output", log::category::render };
		std::uint32_t revision = 0;
		std::uint32_t frames_presented = 0;
		std::uint32_t frames_withheld = 0;
		time waited{};
		interval_timer<> report{ seconds(2.f) };
	};

	struct shared_output_announcement {
		std::uint32_t revision = 0;
		exported_output output;
	};
}
