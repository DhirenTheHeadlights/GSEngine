export module gs:backpack;

import std;
import gse;

import game.config;

class backpack final : public gse::hook<gse::entity> {
public:
	using hook::hook;

	auto initialize() -> void override {
		add_component<gse::physics::motion_component>({
			.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
			.affected_by_gravity = false
		});

		auto model = gse::get<gse::model>("Backpack");
		auto [rc_id, rc] = add_component<gse::render_component>({
			.models = { std::move(model) }
		});
	}
};