export module gs:black_knight;

import std;
import gse;

export namespace gs {
	class black_knight final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			add_component<gse::physics::motion_component>({
				.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
				.affected_by_gravity = false
			});

			auto model = gse::get<gse::model>("Models/BlackKnight/base");
			add_component<gse::render_component>({
				.models = { std::move(model) }
			});
		}
	};
}