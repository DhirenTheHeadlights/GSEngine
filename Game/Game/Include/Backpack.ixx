export module gs:backpack;

import gse;

import game.config;

struct backpack final : gse::hook<gse::entity> {
	using hook::hook;

	auto initialize() -> void override {
		auto model = gse::queue<gse::model>(game::config::resource_path / "Models/BlackKnight/base.obj", "Black Knight");
		auto* render_component = m_scene->registry().add_link<gse::render_component>(owner_id(), model);
		render_component->models[0].set_position(gse::vec::meters(0.f, 0.f, 0.f));
	}
};