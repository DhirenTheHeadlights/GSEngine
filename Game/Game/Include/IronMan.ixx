export module gs:ironman;

import gse;

import game.config;

export namespace gs {
	class iron_man final : gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			auto model = gse::queue<gse::model>(game::config::resource_path / "Models/IronMan/iron_man.obj", "Iron Man");

			auto* render_component = m_scene->registry().add_link<gse::render_component>(owner_id(), model);
			render_component->models[0].set_position(gse::vec::meters(0.f, 0.f, 0.f));
		}
	};
}