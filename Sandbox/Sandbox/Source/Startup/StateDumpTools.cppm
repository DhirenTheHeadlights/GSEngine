export module sandbox:state_dump_tools;

import :startup;

export namespace sandbox::startup {
	auto run_scan_states(
		const config& cfg
	) -> int;

	auto run_compare_states(
		const config& cfg
	) -> int;
}