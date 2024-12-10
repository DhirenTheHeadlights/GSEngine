#include "SphereLight.h"

void Game::SphereLightHook::initialize() {
    const auto lightSourceComponent = std::make_shared<gse::LightSourceComponent>(owner->getId().lock().get());

    lightSourceComponent->addLight(std::make_shared<gse::SpotLight>(
        owner->get_position() + gse::Vec3<gse::Length>(0.f, -(owner->get_radius() + gse::meters(2)), 0.f), gse::Vec3(0.0f, -1.0f, 0.0f), gse::Vec3(1.0f, 1.0f, 1.0f), gse::unitless(1.0f), gse::unitless(1.0f), 0.09f, 0.032f, gse::degrees(45), gse::degrees(65.f), gse::unitless(0.1f)
        ));

    /*lightSourceComponent->addLight(std::make_shared<Engine::PointLight>(
		glm::vec3(1.0f, 1.0f, 1.0f), Engine::unitless(1.0f), glm::vec3(0.f, 0.f, 0.f), Engine::unitless(1.0f), Engine::unitless(0.09f), Engine::unitless(0.032f), Engine::unitless(0.1f)
    ));*/

    owner->addComponent(lightSourceComponent);
}

void Game::SphereLightHook::update() {

}

void Game::SphereLightHook::render() {
    for (const auto& light : owner->getComponent<gse::LightSourceComponent>()->getLights()) {
        light->showDebugMenu(owner->getId().lock());
    }
}