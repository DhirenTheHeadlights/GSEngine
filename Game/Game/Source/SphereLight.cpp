#include "SphereLight.h"

void Game::SphereLightHook::initialize() {
    const auto lightSourceComponent = std::make_shared<Engine::LightSourceComponent>(owner->getId().lock().get());

    lightSourceComponent->addLight(std::make_shared<Engine::SpotLight>(
        owner->getPosition(), Engine::Vec3(0.0f, -1.0f, 0.0f), Engine::Vec3(1.0f, 1.0f, 1.0f), Engine::unitless(1.0f), Engine::unitless(1.0f), 0.09f, 0.032f, Engine::degrees(45), Engine::degrees(65.f), Engine::unitless(0.1f)
        ));

    /*lightSourceComponent->addLight(std::make_shared<Engine::PointLight>(
		glm::vec3(490.0f, 490.0f, 490.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f, 1.f, 0.1f
    ));*/

    owner->addComponent(lightSourceComponent);
}

void Game::SphereLightHook::update() {

}

void Game::SphereLightHook::render() {
    for (const auto& light : owner->getComponent<Engine::LightSourceComponent>()->getLights()) {
        light->showDebugMenu();
    }
}