export module gs:world_loader;

import gse;

import :sandbox_scene;

export namespace gs {
	auto world_loader_setup(gse::engine& e) -> void;
}

auto gs::world_loader_setup(gse::engine& e) -> void {
	auto& w = e.world();
	auto& reg = e.registry();

	gse::add_scene(w, reg, "Sandbox", &sandbox_scene_setup);
}
