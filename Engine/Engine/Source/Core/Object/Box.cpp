#include "Core/Object/Box.h"
#include <imgui.h>

#include "Core/ObjectRegistry.h"
#include "Graphics/RenderComponent.h"

struct box_mesh_hook final : gse::hook<gse::box> {
    box_mesh_hook(gse::box* owner, const gse::vec3<gse::length>& initial_position, const gse::vec3<gse::length>& size)
        : hook(owner), m_initial_position(initial_position), m_size(size) {}

    void initialize() override {
        gse::physics::motion_component new_motion_component(m_owner_id);
        new_motion_component.current_position = m_initial_position;
        new_motion_component.mass = gse::kilograms(100.f);

        gse::physics::collision_component new_collision_component(m_owner_id);
        new_collision_component.bounding_box = { m_initial_position, m_size };

        gse::render_component new_render_component(m_owner_id);

        const float half_width = m_size.as<gse::units::meters>().x / 2.f;
        const float half_height = m_size.as<gse::units::meters>().y / 2.f;
        const float half_depth = m_size.as<gse::units::meters>().z / 2.f;

        const glm::vec3 color = { gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f) };

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

        const std::vector<unsigned int> face_indices = { 0, 1, 2, 2, 3, 0 };

        for (size_t i = 0; i < 6; ++i) {
            gse::mesh new_mesh(face_vertices[i], face_indices);
            new_mesh.set_color(color);
			new_render_component.meshes.push_back(std::move(new_mesh));
        }

        new_render_component.bounding_box_meshes.emplace_back(new_collision_component.bounding_box);

        gse::registry::add_component<gse::physics::motion_component>(std::move(new_motion_component));
        gse::registry::add_component<gse::physics::collision_component>(std::move(new_collision_component));
		gse::registry::add_component<gse::render_component>(std::move(new_render_component));
    }

    void update() override {
	    gse::registry::get_component<gse::render_component>(m_owner_id).set_mesh_positions(gse::registry::get_component<gse::physics::motion_component>(m_owner_id).current_position);
        gse::registry::get_component<gse::physics::collision_component>(m_owner_id).bounding_box.set_position(gse::registry::get_component<gse::physics::motion_component>(m_owner_id).current_position);
    }
    void render() override {
	    gse::debug::add_imgui_callback([this] {
            ImGui::Begin(m_owner->get_id().lock()->tag.c_str());
            ImGui::SliderFloat3("Position", &gse::registry::get_component<gse::physics::motion_component>(m_owner_id).current_position.as_default_units().x, -1000.f, 1000.f);
            ImGui::Text("Colliding: %s", gse::registry::get_component<gse::physics::collision_component>(m_owner_id).bounding_box.collision_information.colliding ? "true" : "false");
            ImGui::End();
            });
    }
private:
	gse::vec3<gse::length> m_initial_position;
	gse::vec3<gse::length> m_size;
};

gse::box::box(const vec3<length>& initial_position, const vec3<length>& size) : object("Box") {
	add_hook(std::make_unique<box_mesh_hook>(this, initial_position, size));
}
