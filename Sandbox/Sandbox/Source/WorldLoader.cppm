export module sandbox:world_loader;

import gse;

import :sandbox_scene;

export namespace sandbox {
	auto world_loader_setup(
		gse::engine& e
	) -> gse::scene*;
}

auto sandbox::world_loader_setup(gse::engine& e) -> gse::scene* {
	auto& w = e.world();
	auto& reg = e.registry();

	gse::add_scene(w, reg, "ParityDrop", &parity_drop_scene_setup);
	gse::add_scene(w, reg, "ParityPair", &parity_pair_scene_setup);
	gse::add_scene(w, reg, "ParityStack", &parity_stack_scene_setup);
	gse::add_scene(w, reg, "ParityPile", &parity_pile_scene_setup);
	gse::add_scene(w, reg, "ParityOverlap", &parity_overlap_scene_setup);
	gse::add_scene(w, reg, "Pyramid", &pyramid_scene_setup);

	return gse::add_scene(w, reg, "Sandbox", &sandbox_scene_setup);
}
