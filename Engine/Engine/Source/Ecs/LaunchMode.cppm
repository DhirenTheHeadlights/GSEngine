export module gse.ecs:launch_mode;

import :system_anno;

export namespace gse::launch {
	struct [[= system_state<"Launch">{}]] data {
		bool presents = true;
		bool render = true;
		bool gpu = true;
		bool simulate_world = true;
	};
}
