#include "Arena.h"

#include <imgui.h>

#include "Graphics/RenderComponent.h"

void Game::Arena::initialize() {
    const gse::length wallThickness = gse::meters(10.f);

    // Create collision component and add bounding boxes for each face
    const auto collisionComponent = std::make_shared<gse::physics::collision_component>(m_id.get());

    // Front and back walls
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(0.f, 0.f, depth / 2.f - wallThickness / 2.f), width, height, wallThickness);
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(0.f, 0.f, -depth / 2.f + wallThickness / 2.f), width, height, wallThickness);

    // Left and right walls
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(-width / 2.f + wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(width / 2.f - wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);

    // Top and bottom walls
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(0.f, height / 2.f - wallThickness / 2.f, 0.f), width, wallThickness, depth);
    collisionComponent->m_bounding_boxes.emplace_back(gse::vec3<gse::Meters>(0.f, -height / 2.f + wallThickness / 2.f, 0.f), width, wallThickness, depth);

    // Colors for each face
    const glm::vec3 colors[] = {
        {1.0f, 0.0f, 0.0f},  // Red - Front
        {0.0f, 1.0f, 0.0f},  // Green - Back
        {0.0f, 0.0f, 1.0f},  // Blue - Left
        {1.0f, 1.0f, 0.0f},  // Yellow - Right
        {1.0f, 0.0f, 1.0f},  // Magenta - Top
        {0.0f, 1.0f, 1.0f}   // Cyan - Bottom
    };

    const float halfWidth  =  width.as<gse::Meters>() / 2.f;
    const float halfHeight =  height.as<gse::Meters>() / 2.f;
    const float halfDepth  =  depth.as<gse::Meters>() / 2.f;

    // Define vertices for each face
    const std::vector<std::vector<gse::vertex>> faceVertices = {
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

    const auto renderComponent = std::make_shared<gse::render_component>(m_id.get());

    // Loop over each face, creating a Mesh and RenderComponent with a unique color
    for (size_t i = 0; i < 6; ++i) {
        auto mesh = std::make_shared<gse::mesh>(faceVertices[i], faceIndices);
		mesh->set_color(colors[i]);
        renderComponent->add_mesh(mesh);

		auto boundingBoxMesh = std::make_shared<gse::bounding_box_mesh>(collisionComponent->m_bounding_boxes[i].lower_bound, collisionComponent->m_bounding_boxes[i].upper_bound);
		renderComponent->add_bounding_box_mesh(boundingBoxMesh);
    }

    add_component(renderComponent);
    add_component(collisionComponent);

    get_component<gse::render_component>()->set_render(true, true);
}

void Game::Arena::update() {

}

void Game::Arena::render() {

}
