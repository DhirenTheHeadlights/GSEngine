#include "SphereLight.h"

#include "Core/ObjectRegistry.h"

void game::sphere_light_hook::initialize() {
	const auto light_source_component = std::make_shared<gse::light_source_component>(m_owner->get_id().lock().get());

    const auto light = std::make_shared<gse::spot_light>(
        gse::vec3(1.f), gse::unitless(250.f), gse::vec3<gse::length>(), gse::vec3(0.0f, -1.0f, 0.0f), gse::unitless(1.0f), 0.09f, 0.032f, gse::degrees(35.f), gse::degrees(65.f), gse::unitless(0.025f)
        );

	const auto ignore_list_id = gse::generate_id("Sphere Light 1 Ignore List");
	gse::registry::add_new_id_list(ignore_list_id, { m_owner->get_id().lock().get() });
	light->set_ignore_list_id(ignore_list_id);

	light_source_component->add_light(light);

	/*light_source_component->add_light(std::make_shared<gse::point_light>(
		gse::vec3(1.f), gse::unitless(1.f), m_owner->get_position(), gse::unitless(1.f), gse::unitless(1.f), gse::unitless(0.09f), gse::unitless(0.032f)
	));*/

    m_owner->add_component(light_source_component);
}

void game::sphere_light_hook::update() {
	m_owner->get_component<gse::light_source_component>()->get_lights().front()->set_position(m_owner->get_component<gse::physics::motion_component>()->current_position);
}

void game::sphere_light_hook::render() {
    for (const auto& light : m_owner->get_component<gse::light_source_component>()->get_lights()) {
        light->show_debug_menu(m_owner->get_id().lock());
    }

	gse::debug::add_imgui_callback([this] {
		ImGui::Begin("Sphere Light");
		ImGui::SliderFloat3("Position", &m_owner->get_component<gse::physics::motion_component>()->current_position.as_default_units().x, -1000.0f, 1000.0f);
		ImGui::End();
		});
}