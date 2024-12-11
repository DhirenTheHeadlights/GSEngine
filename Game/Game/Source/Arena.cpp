#include "Arena.h"

#include <imgui.h>

#include "Graphics/RenderComponent.h"

void game::arena::initialize() {
    const gse::length wall_thickness = gse::meters(10.f);

    // Create collision component and add bounding boxes for each face
    const auto collision_component = std::make_shared<gse::physics::collision_component>(m_id.get());

    // Front and back walls
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(0.f, 0.f, m_depth / 2.f - wall_thickness / 2.f), m_width, m_height, wall_thickness);
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(0.f, 0.f, -m_depth / 2.f + wall_thickness / 2.f), m_width, m_height, wall_thickness);

    // Left and right walls
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(-m_width / 2.f + wall_thickness / 2.f, 0.f, 0.f), wall_thickness, m_height, m_depth);
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(m_width / 2.f - wall_thickness / 2.f, 0.f, 0.f), wall_thickness, m_height, m_depth);

    // Top and bottom walls
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(0.f, m_height / 2.f - wall_thickness / 2.f, 0.f), m_width, wall_thickness, m_depth);
    collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(0.f, -m_height / 2.f + wall_thickness / 2.f, 0.f), m_width, wall_thickness, m_depth);

    // Colors for each face
    const glm::vec3 colors[] = {
        {1.0f, 0.0f, 0.0f},  // Red - Front
        {0.0f, 1.0f, 0.0f},  // Green - Back
        {0.0f, 0.0f, 1.0f},  // Blue - Left
        {1.0f, 1.0f, 0.0f},  // Yellow - Right
        {1.0f, 0.0f, 1.0f},  // Magenta - Top
        {0.0f, 1.0f, 1.0f}   // Cyan - Bottom
    };

    const float half_width  =  m_width.as<gse::units::meters>() / 2.f;
    const float half_height =  m_height.as<gse::units::meters>() / 2.f;
    const float half_depth  =  m_depth.as<gse::units::meters>() / 2.f;

    // Define vertices for each face
    const std::vector<std::vector<gse::vertex>> face_vertices = {
        // Front face
        {
            { glm::vec3(-half_width, -half_height, half_depth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, half_depth), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Back face
        {
            { glm::vec3(-half_width, -half_height, -half_depth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, -half_depth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, -half_depth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Left face
        {
            { glm::vec3(-half_width, -half_height, -half_depth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(-half_width, -half_height, half_depth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(-half_width, half_height, half_depth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Right face
        {
            { glm::vec3(half_width, -half_height, -half_depth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(half_width, half_height, -half_depth), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Top face
        {
            { glm::vec3(-half_width, half_height, half_depth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, -half_depth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Bottom face
        {
            { glm::vec3(-half_width, -half_height, half_depth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, -half_depth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, -half_height, -half_depth), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        }
    };

    // Common indices for each face
    const std::vector<unsigned int> face_indices = { 0, 1, 2, 2, 3, 0 };

    const auto render_component = std::make_shared<gse::render_component>(m_id.get());

    // Loop over each face, creating a Mesh and RenderComponent with a unique color
    for (size_t i = 0; i < 6; ++i) {
        auto mesh = std::make_shared<gse::mesh>(face_vertices[i], face_indices);
		mesh->set_color(colors[i]);
        render_component->add_mesh(mesh);

		auto bounding_box_mesh = std::make_shared<gse::bounding_box_mesh>(collision_component->bounding_boxes[i].lower_bound, collision_component->bounding_boxes[i].upper_bound);
		render_component->add_bounding_box_mesh(bounding_box_mesh);
    }

    add_component(render_component);
    add_component(collision_component);

    get_component<gse::render_component>()->set_render(true, true);
}

void game::arena::update() {

}

void game::arena::render() {

}
