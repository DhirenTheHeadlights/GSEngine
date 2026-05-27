export module gs:world_loader;

import gse;

import :sandbox_scene;

export namespace gs {
	auto world_loader_setup(
		gse::engine& e
	) -> gse::scene*;
}

auto gs::world_loader_setup(gse::engine& e) -> gse::scene* {
	auto& w = e.world();
	auto& reg = e.registry();

	return gse::add_scene(w, reg, "Sandbox", &sandbox_scene_setup);
}
