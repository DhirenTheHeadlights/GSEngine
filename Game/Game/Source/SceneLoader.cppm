export module gs:scene_loader;

import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;

export namespace gs {
	class scene_loader final : public gse::hook<gse::engine> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			auto scene1 = std::make_unique<gse::scene>("Scene1");
			scene1->add_hook<main_test_scene>();

			auto scene2 = std::make_unique<gse::scene>("Scene2");
			scene2->add_hook<skybox_scene>();

			auto scene3 = std::make_unique<gse::scene>("Scene3");
			scene3->add_hook<second_test_scene>();

			gse::scene_loader::add(scene1);
			gse::scene_loader::add(scene2);
			gse::scene_loader::add(scene3);

			gse::scene_loader::queue(gse::find("Scene1"), [] { return gse::keyboard::pressed(gse::key::f1); });
			gse::scene_loader::queue(gse::find("Scene2"), [] { return gse::keyboard::pressed(gse::key::f2); });
			gse::scene_loader::queue(gse::find("Scene3"), [] { return gse::keyboard::pressed(gse::key::f3); });
		}
	};
}
