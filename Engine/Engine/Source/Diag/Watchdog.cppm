export module gse.diag:watchdog;

import std;

import gse.core;
import gse.math;

export namespace gse::watchdog {
	auto start() -> void;

	auto stop() -> void;

	[[nodiscard]] auto dump_pulse() -> std::uint64_t;

	class section : non_copyable, non_movable {
	public:
		section(
			id section_id,
			time budget
		);

		~section();

	private:
		id m_prev_section;
		time m_prev_budget;
		bool m_owned = false;
	};
}

namespace gse::watchdog {
	auto open_stack_capture() -> void;

	auto close_stack_capture() -> void;

	auto capture_all_thread_stacks() -> std::string;
}
