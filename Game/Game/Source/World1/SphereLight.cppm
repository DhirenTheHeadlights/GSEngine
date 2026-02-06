export module gs:sphere_light;

import std;

import gse;

export namespace gs {
	class sphere_light final : public gse::hook<gse::entity> {
	public:
		using params = gse::sphere::params;

		explicit sphere_light(const params& p) : m_position(p.initial_position), m_radius(p.radius), m_sectors(p.sectors), m_stacks(p.stacks) {}

		auto initialize() -> void override {
			add_hook<gse::sphere>({
				.initial_position = m_position,
				.radius = m_radius,
				.sectors = m_sectors,
				.stacks = m_stacks,
			});

			add_component<gse::point_light_component>({
				.color = gse::unitless::vec3(1.f),
				.intensity = 25.f,
				.position = m_position,
				.constant = 1.0f,
				.linear = 0.09f,
				.quadratic = 0.032f,
				.ambient_strength = 0.025f,
				.ignore_list_ids = { owner_id() },
				.near_plane = gse::meters(1.f),
				.far_plane = gse::meters(100.f),
			});

			configure_when_present([](gse::physics::motion_component& mc) {
				mc.affected_by_gravity = false;
				mc.position_locked = true;
			});
		}

		auto render() -> void override {
			component_write<gse::point_light_component>().debug_menu("Sphere Light", owner_id().number());
			component_write<gse::point_light_component>().position = component_read<gse::physics::motion_component>().current_position;
		}
	private:
		static std::size_t m_count;

		gse::vec3<gse::length> m_position;
		gse::length m_radius;
		int m_sectors;
		int m_stacks;
	};
}