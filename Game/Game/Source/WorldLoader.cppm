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

namespace gs {
	struct scene_keys {
		gse::actions::handle default_scene;
		gse::actions::handle skybox_scene;
		gse::actions::handle second_test_scene;
		gse::actions::handle stress_test_scene;
		gse::actions::handle joint_test_scene;
	};

	inline scene_keys g_scene_keys;
}

auto gs::world_loader_setup(gse::engine& e) -> void {
	auto channels = e.make_channel_writer();

	g_scene_keys.default_scene = gse::actions::add<"Load Default Scene">(channels, gse::key::f1);
	g_scene_keys.skybox_scene = gse::actions::add<"Load Skybox Scene">(channels, gse::key::f2);
	g_scene_keys.second_test_scene = gse::actions::add<"Load Second Test Scene">(channels, gse::key::f3);
	g_scene_keys.stress_test_scene = gse::actions::add<"Load Stress Test Scene">(channels, gse::key::f5);
	g_scene_keys.joint_test_scene = gse::actions::add<"Load Joint Test Scene">(channels, gse::key::f6);

	auto& w = e.world();
	auto& reg = e.registry();

	gse::director(&w)
		.when({
			.scene_id = gse::add_scene(w, reg, "Default Scene", &main_test_scene_setup)->id(),
			.condition = [](const gse::evaluation_context& ctx) {
				return gse::actions::pressed(g_scene_keys.default_scene, *ctx.input, *ctx.actions_sys);
			},
		})
		.when({
			.scene_id = gse::add_scene(w, reg, "Skybox Scene", &skybox_scene_setup)->id(),
			.condition = [](const gse::evaluation_context& ctx) {
				return gse::actions::pressed(g_scene_keys.skybox_scene, *ctx.input, *ctx.actions_sys);
			},
		})
		.when({
			.scene_id = gse::add_scene(w, reg, "Second Test Scene", &second_test_scene_setup)->id(),
			.condition = [](const gse::evaluation_context& ctx) {
				return gse::actions::pressed(g_scene_keys.second_test_scene, *ctx.input, *ctx.actions_sys);
			},
		})
		.when({
			.scene_id = gse::add_scene(w, reg, "Physics Stress Test", &physics_stress_test_scene_setup)->id(),
			.condition = [](const gse::evaluation_context& ctx) {
				return gse::actions::pressed(g_scene_keys.stress_test_scene, *ctx.input, *ctx.actions_sys);
			},
		})
		.when({
			.scene_id = gse::add_scene(w, reg, "Physics Joint Test", &physics_joint_test_scene_setup)->id(),
			.condition = [](const gse::evaluation_context& ctx) {
				return gse::actions::pressed(g_scene_keys.joint_test_scene, *ctx.input, *ctx.actions_sys);
			},
		});
}
