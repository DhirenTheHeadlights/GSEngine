export module sandbox:startup;

import std;
import gse;

export namespace sandbox::startup {
	struct config {
		gse::engine_config engine;
		bool physics_parity = false;
		std::size_t physics_parity_envs = 1;
		std::string compare_states_a;
		std::string compare_states_b;
		double compare_states_threshold = 0.01;
		std::string scan_states;
		double scan_states_speed = 20.0;
		std::uint64_t scan_states_body = 0;
		bool scan_states_summary = false;
		double scan_states_settle = 0.1;
		int compare_states_focus_frame = -1;
		int compare_states_focus_bodies = 5;
		int compare_states_focus_window = 4;
		int compare_states_align = 0;
	};
}
