export module gs:skybox;

import std;
import gse;

export namespace gs {
	class skybox final : public gse::hook<gse::entity> {
	public:
		using params = gse::box::params;

		skybox(const params& p) : m_initial_position(p.initial_position), m_size(p.size) {}

		auto initialize() -> void override {
			add_hook<gse::box>({
				.initial_position = m_initial_position,
				.size = m_size
			});

			configure_when_present([](gse::physics::collision_component& cc) {
				cc.resolve_collisions = false;
			});

			configure_when_present([](gse::physics::motion_component& mc) {
				mc.affected_by_gravity = false;
			});

			add_component<gse::directional_light_component>({
				.color = gse::unitless::vec3(1.f),
				.intensity = 1.f,
				.direction = gse::unitless::vec3(0.0f, -1.0f, 0.0f),
				.ambient_strength = 1.0f
			});
		}

		auto render() -> void override {
			component<gse::directional_light_component>().debug_menu("Skybox Light", owner_id().number());
		}
	private:
		gse::vec3<gse::length> m_initial_position;
		gse::vec3<gse::length> m_size;
	};
}