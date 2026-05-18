export module gs:world_loader;

import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;
import :physics_stress_test_scene;
import :physics_joint_test_scene;

export namespace gs {
	auto world_loader_setup(
		gse::engine& e
	) -> void;
}

auto gs::world_loader_setup(gse::engine& e) -> void {
	auto& w = e.world();
	auto& reg = e.registry();

	gse::add_scene(w, reg, "Default Scene", &main_test_scene_setup);
	gse::add_scene(w, reg, "Skybox Scene", &skybox_scene_setup);
	gse::add_scene(w, reg, "Second Test Scene", &second_test_scene_setup);
	gse::add_scene(w, reg, "Physics Stress Test", &physics_stress_test_scene_setup);
	gse::add_scene(w, reg, "Physics Joint Test", &physics_joint_test_scene_setup);
}
