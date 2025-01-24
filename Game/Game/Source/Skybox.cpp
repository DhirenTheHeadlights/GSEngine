module game.skybox;

import std;
import gse;

struct skybox_hook final : gse::hook<gse::entity> {
	using hook::hook;

	auto initialize() -> void override {
		gse::registry::get_component<gse::physics::collision_component>(owner_id).resolve_collisions = false;
		gse::registry::get_component<gse::physics::collision_component>(owner_id).bounding_box = { gse::vec3(0.f), gse::vec3(0.f) };
		gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
		
		gse::registry::get_component<gse::render_component>(owner_id).set_all_mesh_material_strings("Sky 1");

		gse::light_source_component light_source_component(owner_id);
		auto light = std::make_unique<gse::directional_light>(gse::vec3(1.f), gse::unitless(1.f), gse::vec3(0.0f, -1.0f, 0.0f), gse::unitless(1.0f));
		light_source_component.add_light(std::move(light));
		gse::registry::add_component<gse::light_source_component>(std::move(light_source_component));
	}

	auto render() -> void override {
		gse::registry::get_component<gse::light_source_component>(owner_id).get_lights().front()->show_debug_menu("Skybox Light", owner_id);
	}
};

auto game::skybox::create(gse::scene* scene) -> void {
	const auto skybox_position = gse::vec3<gse::units::meters>(0.f, 0.f, 0.f);
	const gse::length skybox_size = gse::meters(20000.f);
	const std::uint32_t skybox_uuid = gse::registry::create_entity();
	create_box(skybox_uuid, skybox_position, skybox_size);
	gse::registry::add_entity_hook(skybox_uuid, std::make_unique<skybox_hook>());
	scene->add_entity(skybox_uuid, "Skybox");
}

