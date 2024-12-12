#include "SphereLight.h"

void game::sphere_light_hook::initialize() {
    const auto light_source_component = std::make_shared<gse::light_source_component>(m_owner->get_id().lock().get());

   light_source_component->add_light(std::make_shared<gse::spot_light>(
        gse::vec3(1.f), gse::unitless(1.f), m_owner->get_position(), gse::vec3(0.0f, -1.0f, 0.0f), gse::unitless(1.0f), 0.09f, 0.032f, gse::degrees(45.f), gse::degrees(65.f), gse::unitless(0.1f)
        ));

	light_source_component->add_light(std::make_shared<gse::point_light>(
		gse::vec3(1.f), gse::unitless(1.f), m_owner->get_position(), gse::unitless(1.f), gse::unitless(1.f), gse::unitless(0.09f), gse::unitless(0.032f)
	));

    m_owner->add_component(light_source_component);
}

void game::sphere_light_hook::update() {
    m_owner->set_position(
        m_owner->get_component<gse::light_source_component>()->get_lights().front()->get_render_queue_entry().shader_entry.position 
        //This block of code will place the light just outside the sphere in the facing direction (i.e., downward facing light placed exactly below the sphere)
        /*+
        (m_owner->get_radius().as_default_unit() * 0.1f) * m_owner->get_component<gse::light_source_component>()->get_lights().front()->get_render_queue_entry().shader_entry.direction*/
    );
}

void game::sphere_light_hook::render() {
    for (const auto& light : m_owner->get_component<gse::light_source_component>()->get_lights()) {
        light->show_debug_menu(m_owner->get_id().lock());
    }
}