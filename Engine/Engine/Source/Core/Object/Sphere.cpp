#include "Core/Object/Sphere.h"
#include <imgui.h>
#include "Graphics/RenderComponent.h"

void Engine::Sphere::initialize() {
    const auto renderComponent = std::make_shared<RenderComponent>();

    const float r = radius.as<Meters>();
    const glm::vec3 posOffset = position.as<Meters>();

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Generate vertices
    for (int stack = 0; stack <= stacks; ++stack) {
	    const float phi = glm::pi<float>() * stack / stacks; // From 0 to PI
	    const float sinPhi = glm::sin(phi);
	    const float cosPhi = glm::cos(phi);

        for (int sector = 0; sector <= sectors; ++sector) {
	        const float theta = 2 * glm::pi<float>() * sector / sectors; // From 0 to 2PI
	        const float sinTheta = glm::sin(theta);
	        const float cosTheta = glm::cos(theta);

            // Calculate vertex position
            glm::vec3 position = {
                r * sinPhi * cosTheta,
                r * cosPhi,
                r * sinPhi * sinTheta
            };

            // Calculate normal (normalized position for a sphere)
	        const glm::vec3 normal = glm::normalize(position);

            // Calculate texture coordinates
	        const glm::vec2 texCoords = {
                static_cast<float>(sector / sectors),
                static_cast<float>(stack / stacks)
            };

            vertices.push_back({ position + posOffset, normal, texCoords });
        }
    }

    // Generate indices
    for (int stack = 0; stack < stacks; ++stack) {
        for (int sector = 0; sector < sectors; ++sector) {
	        const int current = stack * (sectors + 1) + sector;
	        const int next = current + sectors + 1;

            // Two triangles per quad
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    const auto mesh = std::make_shared<Mesh>(vertices, indices);
    renderComponent->addMesh(mesh);
    addComponent(renderComponent);

    const auto lightSourceComponent = std::make_shared<LightSourceComponent>();


    lightSourceComponent->addLight(std::make_shared<SpotLight>(
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 1.0f, 0.09f, 0.032f, 0.f, glm::cos(glm::radians(17.5f)), 0.1f
    ));

    //lightSourceComponent->addLight(std::make_shared<Engine::PointLight>(
    //	glm::vec3(490.0f, 490.0f, 490.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f, 1.f
    //));

    addComponent(lightSourceComponent);
}

void Engine::Sphere::update() {
    getComponent<RenderComponent>()->setRender(true, true);
}

void Engine::Sphere::render() {
    for (const auto& light : getComponent<LightSourceComponent>()->getLights()) {
        light->showDebugMenu();
    }
}
