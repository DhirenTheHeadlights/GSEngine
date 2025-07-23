export module gs:black_knight;

import std;
import gse;

import game.config;

export namespace gs {
	struct black_knight_hook final : gse::hook<gse::entity> {
		using hook::hook;

		auto initialize() -> void override {
			add_component<gse::physics::motion_component>({
				.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
				.affected_by_gravity = false
			});

			auto model = gse::get<gse::model>("BlackKnight");
			auto [rc_id, rc] = add_component<gse::render_component>({
				.models = { std::move(model) }
			});
		}
	};
}