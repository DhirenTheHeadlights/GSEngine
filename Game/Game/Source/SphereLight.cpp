#include "SphereLight.h"

void Game::SphereLightHook::initialize() {
    const auto lightSourceComponent = std::make_shared<gse::light_source_component>(m_owner->get_id().lock().get());

    lightSourceComponent->add_light(std::make_shared<gse::SpotLight>(
        m_owner->get_position() + gse::vec3<gse::length>(0.f, -(m_owner->get_radius() + gse::meters(2)), 0.f), gse::vec3(0.0f, -1.0f, 0.0f), gse::vec3(1.0f, 1.0f, 1.0f), gse::unitless(1.0f), gse::unitless(1.0f), 0.09f, 0.032f, gse::degrees(45), gse::degrees(65.f), gse::unitless(0.1f)
        ));

    /*lightSourceComponent->addLight(std::make_shared<Engine::PointLight>(
		glm::vec3(1.0f, 1.0f, 1.0f), Engine::unitless(1.0f), glm::vec3(0.f, 0.f, 0.f), Engine::unitless(1.0f), Engine::unitless(0.09f), Engine::unitless(0.032f), Engine::unitless(0.1f)
    ));*/

    m_owner->add_component(lightSourceComponent);
}

void Game::SphereLightHook::update() {

}

void Game::SphereLightHook::render() {
    for (const auto& light : m_owner->get_component<gse::light_source_component>()->get_lights()) {
        light->show_debug_menu(m_owner->get_id().lock());
    }
}