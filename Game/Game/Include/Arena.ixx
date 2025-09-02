export module gs:arena;

import std;
import gse;

namespace gs::arena {
	class wall final : public gse::hook<gse::entity> {
	public:
		using params = gse::box::params;

		wall(const params& p) : m_size(p.size), m_initial_position(p.initial_position) {}

		auto initialize() -> void override {
			add_hook<gse::box>({
				.initial_position = m_initial_position,
				.size = m_size
			});

			configure_when_present([](gse::physics::motion_component& mc) {
				mc.affected_by_gravity = false;
				mc.position_locked = true;
			});
		}

		auto update() -> void override {
			component<gse::physics::motion_component>().current_position = m_initial_position;
		}
	private:
		gse::vec3<gse::length> m_size;
		gse::vec3<gse::length> m_initial_position;
	};

	export auto create(const gse::hook<gse::scene>* scene) -> void {
		constexpr auto arena_position = gse::vec3<gse::length>(0.f, 0.f, 0.f);

		constexpr gse::length arena_size = gse::meters(1000.f);
		constexpr gse::length wall_thickness = gse::meters(1.f);

		scene->build("Front Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(0.f, 0.f, arena_size / 2.f),
				.size = gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
			});

		scene->build("Top Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(0.f, arena_size / 2.f, 0.f),
				.size = gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
			});

		scene->build("Bottom Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(0.f, -arena_size / 2.f, 0.f),
				.size = gse::vec3<gse::length>(arena_size, wall_thickness, arena_size)
			});

		scene->build("Left Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(-arena_size / 2.f, 0.f, 0.f),
				.size = gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
			});

		scene->build("Right Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(arena_size / 2.f, 0.f, 0.f),
				.size = gse::vec3<gse::length>(wall_thickness, arena_size, arena_size)
			});

		scene->build("Back Wall")
			.with<wall>({
				.initial_position = arena_position + gse::vec3<gse::length>(0.f, 0.f, -arena_size / 2.f),
				.size = gse::vec3<gse::length>(arena_size, arena_size, wall_thickness)
			});
	}
}