export module gse.core.object.sphere;

import std;

import gse.physics.math;
import gse.core.object.hook;
import gse.core.object_registry;
import gse.graphics.render_component;
import gse.physics.motion_component;
import gse.graphics.debug;
import gse.graphics.model_loader;
import gse.graphics.mesh;
import gse.graphics.texture_loader;

export namespace gse {
    auto create_sphere(std::uint32_t object_uuid, const vec3<length>& position, length radius, int sectors = 36, int stacks = 18) -> void;
    auto create_sphere(const vec3<length>& position, length radius, int sectors = 36, int stacks = 18) -> std::uint32_t;
}

export struct sphere_mesh_hook final : gse::hook<gse::entity> {
    sphere_mesh_hook(const gse::vec3<gse::length>& position, const gse::length radius, const int sectors, const int stacks)
        : m_initial_position(position), m_radius(radius), m_sectors(sectors), m_stacks(stacks) {
    }

    auto initialize() -> void override {
        gse::physics::motion_component new_motion_component(owner_id);
        new_motion_component.current_position = m_initial_position;
        gse::registry::add_component<gse::physics::motion_component>(std::move(new_motion_component));

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
                const gse::unitless::vec3 normal = normalize(position);

                // Calculate texture coordinates
                const gse::unitless::vec2 tex_coords = {
                    static_cast<float>(sector) / static_cast<float>(m_sectors),
                    static_cast<float>(stack) / static_cast<float>(m_stacks)
                };

                vertices.push_back({ .position = position.as<gse::units::meters>(), .normal = normal, .tex_coords = tex_coords});
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

        std::vector<gse::mesh> new_meshes;
        new_meshes.emplace_back(vertices, indices, std::vector<std::uint32_t> { gse::texture_loader::get_texture_by_path("C:/Users/scbel/source/repos/DhirenTheHeadlights/GSEngine/Engine/Resources/Textures/sun.jpg") });

        gse::render_component new_render_component(owner_id, gse::model_loader::add_model(std::move(new_meshes), "Sphere"));

        gse::registry::add_component<gse::render_component>(std::move(new_render_component));
    }

    auto update() -> void override {
        const auto position = gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position;
        gse::registry::get_component<gse::render_component>(owner_id).models[0].set_position(position);
    }
private:
    gse::vec3<gse::length> m_initial_position;
    gse::length m_radius;
    int m_sectors; // Number of slices around the sphere
    int m_stacks;  // Number of stacks from top to bottom
};

auto gse::create_sphere(const std::uint32_t object_uuid, const vec3<length>& position, const length radius, const int sectors, const int stacks) -> void {
    registry::add_entity_hook(object_uuid, std::make_unique<sphere_mesh_hook>(position, radius, sectors, stacks));
}

auto gse::create_sphere(const vec3<length>& position, const length radius, const int sectors, const int stacks) -> std::uint32_t {
    const std::uint32_t sphere_uuid = registry::create_entity();
    create_sphere(sphere_uuid, position, radius, sectors, stacks);
    return sphere_uuid;
}