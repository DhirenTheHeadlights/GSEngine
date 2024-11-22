#include "Core/Object/Box.h"
#include <imgui.h>
#include "Graphics/RenderComponent.h"

void Engine::Box::initialize() {
    const auto renderComponent = std::make_shared<RenderComponent>(id.get());

    const float halfWidth = size.as<Meters>().x / 2.f;
    const float halfHeight = size.as<Meters>().y / 2.f;
    const float halfDepth = size.as<Meters>().z / 2.f;

    const glm::vec3 posOffset = position.as<Meters>();

    const glm::vec3 colors[] = {
        {.75f, 0.5f, 0.2f},  // Front
        {0.2f, 1.0f, 0.3f},  // Back
        {0.3f, 1.0f, 0.8f},  // Left
        {1.0f, 1.0f, 1.0f},  // Right
        {0.3f, 0.3f, 0.8f},  // Top
        {0.2f, 1.0f, 0.3f}   // Bottom
    };

    const std::vector<std::vector<Vertex>> faceVertices = {
        // Front face
        {
            { glm::vec3(-halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Back face
        {
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Left face
        {
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(-halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(-halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Right face
        {
            { glm::vec3(halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Top face
        {
            { glm::vec3(-halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Bottom face
        {
            { glm::vec3(-halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth) + posOffset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth) + posOffset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        }
    };

    const std::vector<unsigned int> faceIndices = { 0, 1, 2, 2, 3, 0 };

    for (size_t i = 0; i < 6; ++i) {
        auto mesh = std::make_shared<Mesh>(faceVertices[i], faceIndices);
        mesh->setColor(colors[i]);
        renderComponent->addMesh(mesh);
    }

	renderComponent->setRender(true, true);

    addComponent(renderComponent);
}