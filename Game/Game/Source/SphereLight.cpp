module;

#include <imgui.h>

module game.sphere_light;

import std;
import gse;

namespace {
	int g_sphere_light_count = 1;
}

struct sphere_light_hook final : gse::hook<gse::entity> {
	using hook::hook;
	auto initialize() -> void override;
	auto update() -> void override;
	auto render() -> void override;
};

auto sphere_light_hook::initialize() -> void {
	gse::light_source_component light_source_component(owner_id);

	auto spot_light = std::make_unique<gse::spot_light>(
		gse::unitless::vec3(1.f), 250.f, gse::vec3<gse::length>(), gse::unitless::vec3(0.0f, -1.0f, 0.0f), 1.0f, 0.09f, 0.032f, gse::degrees(35.f), gse::degrees(65.f), 0.025f
	);

	//auto point_light = std::make_unique<gse::point_light>(
	//gse::vec3(1.f), gse::unitless(250.f), gse::vec3<gse::length>(), gse::unitless(1.f), 0.09f, 0.032f, gse::unitless(0.025f)
	//);

	const auto ignore_list_id = gse::generate_id("Sphere Light " + std::to_string(g_sphere_light_count) + " Ignore List");
	gse::registry::add_new_entity_list(ignore_list_id, { owner_id });
	spot_light->set_ignore_list_id(ignore_list_id);

	light_source_component.add_light(std::move(spot_light));
	//light_source_component.add_light(std::move(point_light));

	gse::registry::add_component<gse::light_source_component>(std::move(light_source_component));

	gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
}

auto sphere_light_hook::update() -> void {
	gse::registry::get_component<gse::light_source_component>(owner_id).get_lights().front()->set_position(gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position);
}

auto sphere_light_hook::render() -> void {
	for (const auto& light : gse::registry::get_component<gse::light_source_component>(owner_id).get_lights()) {
		light->show_debug_menu(gse::registry::get_entity_name(owner_id), owner_id);
	}

	gse::debug::add_imgui_callback([this] {
		ImGui::Begin("Sphere Light");
		ImGui::SliderFloat3("Position", &gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position.x.as_default_unit(), -1000.0f, 1000.0f);
		ImGui::End();
		});
}

auto game::create_sphere_light(const gse::vec3<gse::length>& position, const gse::length& radius, const int sectors) -> std::uint32_t {
	const auto sphere_light_uuid = gse::create_sphere(position, radius, sectors);
	gse::registry::add_entity_hook(sphere_light_uuid, std::make_unique<sphere_light_hook>());
	return sphere_light_uuid;
}