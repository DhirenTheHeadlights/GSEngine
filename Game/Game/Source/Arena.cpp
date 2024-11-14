#include "Arena.h"

#include <imgui.h>

#include "Graphics/RenderComponent.h"

void Game::Arena::initialize() {
    const Engine::Length wallThickness = Engine::meters(10.f);

    // Create collision component and add bounding boxes for each face
    const auto collisionComponent = std::make_shared<Engine::Physics::CollisionComponent>();

    // Front and back walls
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(0.f, 0.f, depth / 2.f - wallThickness / 2.f), width, height, wallThickness);
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(0.f, 0.f, -depth / 2.f + wallThickness / 2.f), width, height, wallThickness);

    // Left and right walls
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(-width / 2.f + wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(width / 2.f - wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);

    // Top and bottom walls
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(0.f, height / 2.f - wallThickness / 2.f, 0.f), width, wallThickness, depth);
    collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(0.f, -height / 2.f + wallThickness / 2.f, 0.f), width, wallThickness, depth);

    // Colors for each face
    const glm::vec3 colors[] = {
        {1.0f, 0.0f, 0.0f},  // Red - Front
        {0.0f, 1.0f, 0.0f},  // Green - Back
        {0.0f, 0.0f, 1.0f},  // Blue - Left
        {1.0f, 1.0f, 0.0f},  // Yellow - Right
        {1.0f, 0.0f, 1.0f},  // Magenta - Top
        {0.0f, 1.0f, 1.0f}   // Cyan - Bottom
    };

    const float halfWidth  =  width.as<Engine::Meters>() / 2.f;
    const float halfHeight =  height.as<Engine::Meters>() / 2.f;
    const float halfDepth  =  depth.as<Engine::Meters>() / 2.f;

    // Define vertices for each face
    const std::vector<std::vector<Engine::Vertex>> faceVertices = {
        // Front face
        {
            { glm::vec3(-halfWidth, -halfHeight, halfDepth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, halfDepth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Back face
        {
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, -halfDepth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Left face
        {
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(-halfWidth, -halfHeight, halfDepth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(-halfWidth, halfHeight, halfDepth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Right face
        {
            { glm::vec3(halfWidth, -halfHeight, -halfDepth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Top face
        {
            { glm::vec3(-halfWidth, halfHeight, halfDepth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, halfDepth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, halfHeight, -halfDepth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, halfHeight, -halfDepth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Bottom face
        {
            { glm::vec3(-halfWidth, -halfHeight, halfDepth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, halfDepth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(halfWidth, -halfHeight, -halfDepth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-halfWidth, -halfHeight, -halfDepth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        }
    };

    // Common indices for each face
    const std::vector<unsigned int> faceIndices = { 0, 1, 2, 2, 3, 0 };

    const auto renderComponent = std::make_shared<Engine::RenderComponent>();

    // Loop over each face, creating a Mesh and RenderComponent with a unique color
    for (size_t i = 0; i < 6; ++i) {
        auto mesh = std::make_shared<Engine::Mesh>(faceVertices[i], faceIndices);
		mesh->setColor(colors[i]);
        renderComponent->addMesh(mesh);

		auto boundingBoxMesh = std::make_shared<Engine::BoundingBoxMesh>(collisionComponent->boundingBoxes[i].lowerBound, collisionComponent->boundingBoxes[i].upperBound);
		renderComponent->addBoundingBoxMesh(boundingBoxMesh);
    }

    addComponent(renderComponent);
    addComponent(collisionComponent);

    const auto lightSourceComponent = std::make_shared<Engine::LightSourceComponent>();


    lightSourceComponent->addLight(std::make_shared<Engine::SpotLight>(
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 1.0f, 0.09f, 0.032f, 0.f, glm::cos(glm::radians(17.5f))
    ));
 //   lightSourceComponent->addLight(std::make_shared<Engine::PointLight>(
	//	glm::vec3(490.0f, 490.0f, 490.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f, 1.f
	//));

	addComponent(lightSourceComponent);
}

void Game::Arena::update() {
    getComponent<Engine::RenderComponent>()->setRender(true, true);
}

void Game::Arena::render() {
    for (const auto& light : getComponent<Engine::LightSourceComponent>()->getLights()) {
        //light->showDebugMenu();
    }
}
