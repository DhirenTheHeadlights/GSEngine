#include "Skybox.h"

#include "Core/ObjectRegistry.h"

struct skybox_hook final : gse::hook<gse::box> {
	using hook::hook;

	void initialize() override {
		gse::registry::get_component<gse::physics::collision_component>(m_owner_id).resolve_collisions = false;
		gse::registry::get_component<gse::physics::motion_component>(m_owner_id).affected_by_gravity = false;
		
		gse::registry::get_component<gse::render_component>(m_owner_id).set_all_mesh_material_strings("Sky 1");

		gse::light_source_component light_source_component(m_owner_id);
		auto light = std::make_unique<gse::directional_light>(gse::vec3(1.f), gse::unitless(1.f), gse::vec3(0.0f, -1.0f, 0.0f), gse::unitless(1.0f));
		light_source_component.add_light(std::move(light));
		gse::registry::add_component<gse::light_source_component>(std::move(light_source_component));
	}
};

void game::skybox::create(gse::scene* scene) {
	const auto skybox_position = gse::vec3<gse::units::meters>(0.f, 0.f, 0.f);
	const gse::length skybox_size = gse::meters(2000.f);
	auto skybox = std::make_unique<gse::box>(skybox_position, skybox_size);
	skybox->add_hook(std::make_unique<skybox_hook>(skybox.get()));
	scene->add_object(std::move(skybox));
}

