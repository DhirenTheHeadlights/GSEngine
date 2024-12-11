
#include "Core/Object/Sphere.h"
#include <imgui.h>
#include "Graphics/RenderComponent.h"

void gse::sphere::initialize() {
    const auto new_render_component = std::make_shared<render_component>(m_id.get());

    const float r = m_radius.as<units::meters>();
    const glm::vec3 pos_offset = m_position.as<units::meters>();

    std::vector<vertex> vertices;
    std::vector<unsigned int> indices;

    // Generate vertices
    for (int stack = 0; stack <= m_stacks; ++stack) {
        const float phi = glm::pi<float>() * (static_cast<float>(stack) / static_cast<float>(m_stacks)); // From 0 to PI
        const float sin_phi = glm::sin(phi);
        const float cos_phi = glm::cos(phi);

        for (int sector = 0; sector <= m_sectors; ++sector) {
            const float theta = 2 * glm::pi<float>() * (static_cast<float>(sector) / static_cast<float>(m_sectors)); // From 0 to 2PI
            const float sin_theta = glm::sin(theta);
            const float cos_theta = glm::cos(theta);

            // Calculate vertex position
            glm::vec3 position = {
                r * sin_phi * cos_theta,
                r * cos_phi,
                r * sin_phi * sin_theta
            };

            // Calculate normal (normalized position for a sphere)
            const glm::vec3 normal = glm::normalize(position);

            // Calculate texture coordinates
            const glm::vec2 tex_coords = {
                static_cast<float>(sector) / static_cast<float>(m_sectors),
                static_cast<float>(stack) / static_cast<float>(m_stacks)
            };

            vertices.push_back({ position + pos_offset, normal, tex_coords });
        }
    }

    // Generate indices
    for (int stack = 0; stack < m_stacks; ++stack) {
        for (int sector = 0; sector < m_sectors; ++sector) {
	        const int current = stack * (m_sectors + 1) + sector;
	        const int next = current + m_sectors + 1;

            // Two triangles per quad
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    const auto new_mesh = std::make_shared<mesh>(vertices, indices);
    new_render_component->add_mesh(new_mesh);
    add_component(new_render_component);

    get_component<render_component>()->set_render(true, true);
}