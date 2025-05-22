module;

#include <imgui.h>

export module gse.core.object.box;

import std;

import gse.core.config;
import gse.physics.collision_component;
import gse.physics.motion_component;
import gse.physics.math;
import gse.core.object.hook;
import gse.core.object_registry;
import gse.graphics.render_component;
import gse.graphics.mesh;
import gse.graphics.model_loader;
import gse.graphics.debug;
import gse.graphics.texture_loader;

export namespace gse {
    auto create_box(std::uint32_t object_uuid, const vec3<length>& initial_position, const vec3<length>& size) -> void;
    auto create_box(const vec3<length>& initial_position, const vec3<length>& size) -> std::uint32_t;
}

export struct box_mesh_hook final : gse::hook<gse::entity> {
    box_mesh_hook(const gse::vec3<gse::length>& initial_position, const gse::vec3<gse::length>& size)
        : m_initial_position(initial_position), m_size(size) {
    }

    auto initialize() -> void override {
        gse::physics::motion_component new_motion_component(owner_id);
        new_motion_component.current_position = m_initial_position;
        new_motion_component.mass = gse::kilograms(100.f);

        gse::physics::collision_component new_collision_component(owner_id);
        new_collision_component.bounding_box = { m_initial_position, m_size };
        new_collision_component.oriented_bounding_box = { new_collision_component.bounding_box };

        const float half_width = m_size.as<gse::units::meters>().x / 2.f;
        const float half_height = m_size.as<gse::units::meters>().y / 2.f;
        const float half_depth = m_size.as<gse::units::meters>().z / 2.f;

        const gse::unitless::vec3 color = { gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f) };

        const float repeat_interval = 10.f; // 1 repeat per unit (meters in this case)

        const std::vector<std::vector<gse::vertex>> face_vertices = {
            // Front face
            {
                {.position = {-half_width, -half_height, half_depth}, .normal = {0.0f, 0.0f, repeat_interval}, .tex_coords = {0.0f, 0.0f} },
                {.position = {half_width, -half_height, half_depth}, .normal = {0.0f, 0.0f, repeat_interval}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {half_width, half_height, half_depth}, .normal = {0.0f, 0.0f, repeat_interval}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {-half_width, half_height, half_depth}, .normal = {0.0f, 0.0f, repeat_interval}, .tex_coords = {0.0f, repeat_interval} }
            },
            // Back face
            {
                {.position = {-half_width, -half_height, -half_depth}, .normal = {0.0f, 0.0f, -repeat_interval}, .tex_coords = {0.0f, 0.0f} },
                {.position = {half_width, -half_height, -half_depth}, .normal = {0.0f, 0.0f, -repeat_interval}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {half_width, half_height, -half_depth}, .normal = {0.0f, 0.0f, -repeat_interval}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {-half_width, half_height, -half_depth}, .normal = {0.0f, 0.0f, -repeat_interval}, .tex_coords = {0.0f, repeat_interval} }
            },
            // Left face
            {
                {.position = {-half_width, -half_height, -half_depth}, .normal = {-repeat_interval, 0.0f, 0.0f}, .tex_coords = {0.0f, 0.0f} },
                {.position = {-half_width, -half_height, half_depth}, .normal = {-repeat_interval, 0.0f, 0.0f}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {-half_width, half_height, half_depth}, .normal = {-repeat_interval, 0.0f, 0.0f}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {-half_width, half_height, -half_depth}, .normal = {-repeat_interval, 0.0f, 0.0f}, .tex_coords = {0.0f, repeat_interval} }
            },
            // Right face
            {
                {.position = {half_width, -half_height, -half_depth}, .normal = {repeat_interval, 0.0f, 0.0f}, .tex_coords = {0.0f, 0.0f} },
                {.position = {half_width, -half_height, half_depth}, .normal = {repeat_interval, 0.0f, 0.0f}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {half_width, half_height, half_depth}, .normal = {repeat_interval, 0.0f, 0.0f}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {half_width, half_height, -half_depth}, .normal = {repeat_interval, 0.0f, 0.0f}, .tex_coords = {0.0f, repeat_interval} }
            },
            // Top face
            {
                {.position = {-half_width, half_height, half_depth}, .normal = {0.0f, repeat_interval, 0.0f}, .tex_coords = {0.0f, 0.0f} },
                {.position = {half_width, half_height, half_depth}, .normal = {0.0f, repeat_interval, 0.0f}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {half_width, half_height, -half_depth}, .normal = {0.0f, repeat_interval, 0.0f}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {-half_width, half_height, -half_depth}, .normal = {0.0f, repeat_interval, 0.0f}, .tex_coords = {0.0f, repeat_interval} }
            },
            // Bottom face
            {
                {.position = {-half_width, -half_height, half_depth}, .normal = {0.0f, -repeat_interval, 0.0f}, .tex_coords = {0.0f, 0.0f} },
                {.position = {half_width, -half_height, half_depth}, .normal = {0.0f, -repeat_interval, 0.0f}, .tex_coords = {repeat_interval, 0.0f} },
                {.position = {half_width, -half_height, -half_depth}, .normal = {0.0f, -repeat_interval, 0.0f}, .tex_coords = {repeat_interval, repeat_interval} },
                {.position = {-half_width, -half_height, -half_depth}, .normal = {0.0f, -repeat_interval, 0.0f}, .tex_coords = {0.0f, repeat_interval} }
            }
        };

        std::vector<std::uint32_t> face_indices = { 0, 1, 2, 2, 3, 0 };

        std::vector<gse::mesh> meshes;

        for (size_t i = 0; i < 6; ++i) {
            meshes.emplace_back(face_vertices[i], face_indices, std::vector { gse::texture_loader::get_texture_by_path(gse::config::resource_path / "Textures/concrete_bricks_1.jpg") });
        }

        gse::render_component new_render_component(owner_id, gse::model_loader::add_model(std::move(meshes), "Box"));

        gse::registry::add_component<gse::physics::motion_component>(std::move(new_motion_component));
        gse::registry::add_component<gse::physics::collision_component>(std::move(new_collision_component));
        gse::registry::add_component<gse::render_component>(std::move(new_render_component));
    }

    auto update() -> void override {
        gse::registry::get_component<gse::render_component>(owner_id).models[0].set_position(gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position);
        gse::registry::get_component<gse::physics::collision_component>(owner_id).bounding_box.set_position(gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position);
    }

    auto render() -> void override {
        gse::debug::add_imgui_callback([this] {
            auto& render_component = gse::registry::get_component<gse::render_component>(owner_id);
            ImGui::Begin(gse::registry::get_entity_name(owner_id).data());
            ImGui::SliderFloat3("Position", &gse::registry::get_component<gse::physics::motion_component>(owner_id).current_position.x.as_default_unit(), -1000.f, 1000.f);
            ImGui::Text("Colliding: %s", gse::registry::get_component<gse::physics::collision_component>(owner_id).collision_information.colliding ? "true" : "false");
            ImGui::Text("Center of Mass: %f, %f, %f", render_component.center_of_mass.x.as_default_unit(), render_component.center_of_mass.y.as_default_unit(), render_component.center_of_mass.z.as_default_unit());
            ImGui::End();
            });
    }
private:
    gse::vec3<gse::length> m_initial_position;
    gse::vec3<gse::length> m_size;
};

auto gse::create_box(const std::uint32_t object_uuid, const vec3<length>& initial_position, const vec3<length>& size) -> void {
    registry::add_entity_hook(object_uuid, std::make_unique<box_mesh_hook>(initial_position, size));
}

auto gse::create_box(const vec3<length>& initial_position, const vec3<length>& size) -> std::uint32_t {
    const std::uint32_t box_uuid = registry::create_entity();
    create_box(box_uuid, initial_position, size);
    return box_uuid;
}
