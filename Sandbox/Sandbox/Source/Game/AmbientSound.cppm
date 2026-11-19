export module sandbox:ambient_sound;

import std;
import gse;

export namespace sandbox::ambient_sound {
	struct component {
		std::string_view clip;
		gse::percentage<float> gain = gse::percentage<float>::one();
	};

	struct [[= gse::system_state<"AmbientSound">{}]]
	data {
		std::vector<gse::id> waiting;
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		gse::write<component> sounds,
		gse::shared_view<gse::asset::data> assets_d,
		gse::channel_write<gse::audio::play_request> audio_out
	) -> gse::async::task<>;
}
