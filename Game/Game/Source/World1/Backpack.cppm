export module gs:backpack;

import std;
import gse;

class backpack final : public gse::hook<gse::entity> {
public:
	using hook::hook;

	auto initialize() -> void override {
		add_component<gse::physics::motion_component>({
			.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
			.affected_by_gravity = false
		});

		add_component<gse::render_component>({
			.models = {
				gse::get<gse::model>("Backpack")
			}
		});
	}

	auto update() -> void override {
		
	}
};