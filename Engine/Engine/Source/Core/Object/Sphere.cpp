#include "Core/Object/Sphere.h"

#include <imgui.h>
#include "Graphics/RenderComponent.h"

struct sphere_mesh_hook final : gse::hook<gse::sphere> {
	sphere_mesh_hook(gse::sphere* owner, const gse::vec3<gse::length>& position, const gse::length radius, const int sectors, const int stacks)
		: hook(owner), m_initial_position(position), m_radius(radius), m_sectors(sectors), m_stacks(stacks) {}

	void initialize() override {
		const auto id = m_owner->get_id().lock();
        const auto new_motion_component = std::make_shared<gse::physics::motion_component>(id.get());
        new_motion_component->current_position = m_initial_position;
        m_owner->add_component(new_motion_component);

        const auto new_render_component = std::make_shared<gse::render_component>(id.get());

        const float r = m_radius.as<gse::units::meters>();

        std::vector<gse::vertex> vertices;
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

                vertices.push_back({ position, normal, tex_coords });
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

        const auto new_mesh = std::make_shared<gse::mesh>(vertices, indices);
        new_render_component->add_mesh(new_mesh);
		new_render_component->set_render(true, true);
        m_owner->add_component(new_render_component);
	}

	void update() override {
        const auto position = m_owner->get_component<gse::physics::motion_component>()->current_position.as<gse::units::meters>();

        m_owner->get_component<gse::render_component>()->set_mesh_positions(position);
	}
private:
	gse::vec3<gse::length> m_initial_position;
	gse::length m_radius;
    int m_sectors; // Number of slices around the sphere
    int m_stacks;  // Number of stacks from top to bottom
};

gse::sphere::sphere(const vec3<length>& position, const length radius, const int sectors, const int stacks) : object("Sphere") {
	add_hook(std::make_unique<sphere_mesh_hook>(this, position, radius, sectors, stacks));
}
