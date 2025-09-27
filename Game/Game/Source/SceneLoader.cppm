export module gs:scene_loader;

import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;

export namespace gs {
	class scene_loader final : public gse::hook<gse::world> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			m_owner->set_networked(true);

			m_owner->direct()
				.when({
					.scene_id = m_owner->add<main_test_scene>("Default Scene")->id(),
					.condition = [](const gse::evaluation_context& ctx) {
						return ctx.input.key_pressed(gse::key::f1);
					}
				})
				.when({
					.scene_id = m_owner->add<skybox_scene>("Skybox Scene")->id(),
					.condition = [](const gse::evaluation_context& ctx) {
						return ctx.input.key_pressed(gse::key::f2);
					}
				})
				.when({
					.scene_id = m_owner->add<second_test_scene>("Second Test Scene")->id(),
					.condition = [](const gse::evaluation_context& ctx) {
						return ctx.input.key_pressed(gse::key::f3);
					}
				});
		}
	};
}
