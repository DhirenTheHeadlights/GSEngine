export module sandbox:world_loader;

import std;
import gse;

import :world_catalog;

export namespace sandbox {
	auto world_loader_setup(
		gse::engine& e
	) -> void;
}

auto sandbox::world_loader_setup(gse::engine& e) -> void {
	auto& w = e.world();
	auto& reg = e.registry();

	for (const world_entry& entry : world_catalog()) {
		gse::add_scene(w, reg, entry.name, entry.setup);
	}
}
