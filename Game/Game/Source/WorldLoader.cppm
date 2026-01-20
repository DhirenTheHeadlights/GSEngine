export module gs:world_loader;
import gse;
import :main_test_scene;
import :skybox_scene;
import :second_test_scene;
import :animation_test_scene;

export namespace gs {
	class world_loader final : public gse::hook<gse::engine> {
	public:
		using hook::hook;
		
		auto initialize() -> void override {
			m_default_scene_key = gse::actions::add<"Load Default Scene">(gse::key::f1);
			m_skybox_scene_key = gse::actions::add<"Load Skybox Scene">(gse::key::f2);
			m_second_test_scene_key = gse::actions::add<"Load Second Test Scene">(gse::key::f3);
			m_animation_test_scene_key = gse::actions::add<"Load Animation Test Scene">(gse::key::f4);
			
			m_owner->direct()
				.when({
					.scene_id = m_owner->add_scene<main_test_scene>("Default Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return gse::actions::pressed(m_default_scene_key, *ctx.input);
					}
				})
				.when({
					.scene_id = m_owner->add_scene<skybox_scene>("Skybox Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return gse::actions::pressed(m_skybox_scene_key, *ctx.input);
					}
				})
				.when({
					.scene_id = m_owner->add_scene<second_test_scene>("Second Test Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return gse::actions::pressed(m_second_test_scene_key, *ctx.input);
					}
				})
				.when({
					.scene_id = m_owner->add_scene<animation_test_scene>("Animation Test Scene")->id(),
					.condition = [&](const gse::evaluation_context& ctx) {
						return gse::actions::pressed(m_animation_test_scene_key, *ctx.input);
					}
				});
		}
		
	private:
		gse::actions::handle m_default_scene_key;
		gse::actions::handle m_skybox_scene_key;
		gse::actions::handle m_second_test_scene_key;
		gse::actions::handle m_animation_test_scene_key;
	};
}