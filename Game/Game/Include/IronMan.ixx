export module gs:ironman;

import std;
import gse;

import game.config;

export namespace gs {
	class iron_man final : gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			add_component<gse::physics::motion_component>({
				.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
				.affected_by_gravity = false
			});

			auto model = gse::get<gse::model>("IronMan");
			auto [rc_id, rc] = add_component<gse::render_component>({
				.models = { std::move(model) }
			});
		}
	};
}