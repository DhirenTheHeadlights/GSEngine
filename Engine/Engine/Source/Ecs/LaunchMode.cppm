export module gse.ecs:launch_mode;

import std;

import :shared_view;
import :system_anno;

export namespace gse::launch {
	enum class audio_output : std::uint8_t {
		none,
		device,
		offline,
	};

	struct [[= system_state<"Launch">{}]] data {
		bool presents = true;
		bool render = true;
		bool gpu = true;
		bool simulate_world = true;
		[[= shared]]
		audio_output audio = audio_output::device;
	};
}
