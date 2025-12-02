export module gs:world_loader;

import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;

export namespace gs {
	class world_loader final : public gse::hook<gse::engine> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			m_default_scene_key = gse::actions::add<"Load Default Scene">(gse::key::f1);
			m_skybox_scene_key = gse::actions::add<"Load Skybox Scene">(gse::key::f2);
			m_second_test_scene_key = gse::actions::add<"Load Second Test Scene">(gse::key::f3);

			gse::actions::finalize_bindings();

			m_owner->world.direct()
				.when({
					.scene_id = m_owner->world.add<main_test_scene>("Default Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return ctx.input->pressed(m_default_scene_key);
					}
				})
				.when({
					.scene_id = m_owner->world.add<skybox_scene>("Skybox Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return ctx.input->pressed(m_skybox_scene_key);
					}
				})
				.when({
					.scene_id = m_owner->world.add<second_test_scene>("Second Test Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return ctx.input->pressed(m_second_test_scene_key);
					}
				});
		}
	private:
		gse::id m_default_scene_key, m_skybox_scene_key, m_second_test_scene_key;
	};
}
