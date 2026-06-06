export module gse.diag:watchdog;

import std;

import gse.core;
import gse.math;

export namespace gse::watchdog {
	auto start() -> void;

	auto stop() -> void;

	[[nodiscard]] auto dump_pulse() -> std::uint64_t;

	class section {
	public:
		section(
			id section_id,
			time budget
		);

		~section();

		section(
			const section&
		) = delete;

		section(
			section&&
		) = delete;

		auto operator=(
			const section&
		) -> section& = delete;

		auto operator=(
			section&&
		) -> section& = delete;

	private:
		id m_prev_section;
		time m_prev_budget;
	};
}
