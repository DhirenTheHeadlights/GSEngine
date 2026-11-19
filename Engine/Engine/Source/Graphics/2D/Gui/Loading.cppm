export module gse.graphics:loading;

import gse.diag;
import gse.log;
import std;

export namespace gse::loading {
	struct boot_stage_info {
		char label[48];
	};

	enum class boot_stage : std::uint8_t {
		configuring,
		initializing [[
			= entered_from({ configuring })
		]],
		presenting [[
			= entered_from({ initializing }),
			= boot_stage_info{
				.label = "Starting",
			}
		]],
		app_setup [[
			= entered_from({ presenting }),
			= boot_stage_info{
				.label = "Loading application",
			}
		]],
		settling [[
			= entered_from({ initializing, app_setup }),
			= boot_stage_info{
				.label = "Starting systems",
			}
		]],
		ready [[
			= entered_from({ settling })
		]],
	};

	struct state {
		auto set_progress(
			std::uint32_t settled,
			std::uint32_t expected
		) -> void;

		std::uint32_t done = 0;
		std::uint32_t total = 0;
		bool awaiting_first_draw = false;
	};
}

auto gse::loading::state::set_progress(const std::uint32_t settled, const std::uint32_t expected) -> void {
	if (settled == done && expected == total) {
		return;
	}
	done = settled;
	total = expected;
	log::println(log::category::runtime, "loading::set_progress done={} total={}", done, total);
}
