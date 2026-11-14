module gse.runtime:restart_policy;

import std;

namespace gse {
	auto install_restart_policy(
		bool restart_on_crash
	) -> void;

	auto arm_restart(
		std::string_view cause
	) -> bool;

	[[nodiscard]] auto device_lost_simulation_requested() -> bool;
}
