export module gs:sphere_light;

import std;

import gse;

export namespace gs {
	class sphere_light_hook final : gse::hook<gse::entity> {
	public:
		using hook::hook;
		auto initialize() -> void override {
			auto lsc = m_scene->registry().add_link<gse::light_source_component>(owner_id());

			auto spot_light = std::make_unique<gse::spot_light>(
				gse::unitless::vec3(1.f),
				250.f,
				gse::vec3<gse::length>(),
				gse::unitless::vec3(0.0f, -1.0f, 0.0f),
				1.0f,
				0.09f,
				0.032f,
				gse::degrees(35.f),
				gse::degrees(65.f),
				0.025f
			);

			const auto ignore_list_id = gse::generate_id("Sphere Light " + std::to_string(m_count++) + " Ignore List");
			gse::registry::add_new_entity_list(ignore_list_id, { owner_id });
			spot_light->set_ignore_list_id(ignore_list_id);

			light_source_component.add_light(std::move(spot_light));
			//light_source_component.add_light(std::move(point_light));

			gse::registry::add_component<gse::light_source_component>(std::move(light_source_component));

			gse::registry::component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
		}
		auto update() -> void override;
		auto render() -> void override;
	private:
		static std::size_t m_count;
	};

	std::size_t sphere_light_hook::m_count = 0;
}