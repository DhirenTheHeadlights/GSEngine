export module gs:second_test_scene;

import std;
import gse;

export namespace gs {
	auto second_test_scene_setup(
		gse::scene& s,
		gse::channel_writer& channels,
		gse::asset::data& assets
	) -> void;
}

auto gs::second_test_scene_setup(gse::scene& s, gse::channel_writer& channels, gse::asset::data& assets) -> void {
	(void)s;
	(void)channels;
	(void)assets;
}
