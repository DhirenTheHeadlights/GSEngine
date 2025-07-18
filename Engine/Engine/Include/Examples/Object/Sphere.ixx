export module gse.examples:sphere;

import std;

import gse.runtime;
import gse.utility;
import gse.graphics;
import gse.physics;

export class sphere_mesh_hook final : gse::hook<gse::entity> {
public:
    sphere_mesh_hook(const gse::id& owner_id, gse::scene* scene, const gse::vec3<gse::length>& position, const gse::length radius, const int sectors, const int stacks)
        : hook(owner_id, scene), m_initial_position(position), m_radius(radius), m_sectors(sectors), m_stacks(stacks) {
    }

    auto initialize() -> void override {
        auto* mc = m_scene->registry().add_link<gse::physics::motion_component>(owner_id());
        mc->current_position = m_initial_position;

        const float r = m_radius.as<gse::units::meters>();

        std::vector<gse::vertex> vertices;
        std::vector<std::uint32_t> indices;

        // Generate vertices
        for (int stack = 0; stack <= m_stacks; ++stack) {
            const float phi = std::numbers::pi_v<float> *(static_cast<float>(stack) / static_cast<float>(m_stacks)); // From 0 to PI
            const float sin_phi = std::sin(phi);
            const float cos_phi = std::cos(phi);

            for (int sector = 0; sector <= m_sectors; ++sector) {
                const float theta = 2 * std::numbers::pi_v<float> *(static_cast<float>(sector) / static_cast<float>(m_sectors)); // From 0 to 2PI
                const float sin_theta = std::sin(theta);
                const float cos_theta = std::cos(theta);

                // Calculate vertex position
                gse::vec3<gse::length> position = {
                    r * sin_phi * cos_theta,
                    r * cos_phi,
                    r * sin_phi * sin_theta
                };

                // Calculate normal (normalized position for a sphere)
                const gse::unitless::vec3 normal = gse::normalize(position);

                // Calculate texture coordinates
                const gse::unitless::vec2 tex_coords = {
                    static_cast<float>(sector) / static_cast<float>(m_sectors),
                    static_cast<float>(stack) / static_cast<float>(m_stacks)
                };

                vertices.push_back({ .position = position.as<gse::units::meters>().storage, .normal = normal.storage, .tex_coords = tex_coords.storage });
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

        std::vector<gse::mesh_data> new_meshes;
        new_meshes.emplace_back(
            vertices,
            indices,
            gse::queue<gse::material>(
                "sun_material",
                gse::queue<gse::texture>(gse::config::resource_path / "Textures/sun.jpg", "sun")
            )
        );

        m_scene->registry().add_link<gse::render_component>(owner_id(), gse::queue<gse::model>("Sphere", std::move(new_meshes)));
    }

    auto update() -> void override {
        component<gse::render_component>().models[0].set_position(component<gse::physics::motion_component>().current_position);
    }
private:
    gse::vec3<gse::length> m_initial_position;
    gse::length m_radius;
    int m_sectors; // Number of slices around the sphere
    int m_stacks;  // Number of stacks from top to bottom
};