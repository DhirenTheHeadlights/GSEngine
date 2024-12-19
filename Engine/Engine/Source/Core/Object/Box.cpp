#include "Core/Object/Box.h"
#include <imgui.h>
#include "Graphics/RenderComponent.h"

struct box_mesh_hook final : gse::hook<gse::box> {
    box_mesh_hook(gse::box* owner, const gse::vec3<gse::length>& initial_position, const gse::vec3<gse::length>& size)
        : hook(owner), m_initial_position(initial_position), m_size(size) {}

    void initialize() override {
		const auto id = m_owner->get_id().lock();

        const auto new_motion_component = std::make_shared<gse::physics::motion_component>(id.get());
        new_motion_component->current_position = m_initial_position;
        new_motion_component->mass = gse::kilograms(100.f);
        m_owner->add_component(new_motion_component);

        const auto new_collision_component = std::make_shared<gse::physics::collision_component>(id.get());
        new_collision_component->bounding_boxes.emplace_back(m_initial_position, m_size);
        m_owner->add_component(new_collision_component);

        const auto new_render_component = std::make_shared<gse::render_component>(id.get());

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
            const auto new_mesh = std::make_shared<gse::mesh>(face_vertices[i], face_indices);
            new_mesh->set_color(color);
            new_render_component->add_mesh(new_mesh);
        }

        new_render_component->set_render(true, true);

        const auto new_bounding_box_mesh = std::make_shared<gse::bounding_box_mesh>(m_owner->get_component<gse::physics::collision_component>()->bounding_boxes[0]);
        new_render_component->add_bounding_box_mesh(new_bounding_box_mesh);

        m_owner->add_component(new_render_component);
    }

    void update() override {
        m_owner->get_component<gse::render_component>()->set_mesh_positions(m_owner->get_component<gse::physics::motion_component>()->current_position);
        m_owner->get_component<gse::physics::collision_component>()->bounding_boxes[0].set_position(m_owner->get_component<gse::physics::motion_component>()->current_position);
    }
    void render() override {
	    gse::debug::add_imgui_callback([this] {
            ImGui::Begin(m_owner->get_id().lock()->tag.c_str());
            ImGui::SliderFloat3("Position", &m_owner->get_component<gse::physics::motion_component>()->current_position.as_default_units().x, -1000.f, 1000.f);
            ImGui::Text("Colliding: %s", m_owner->get_component<gse::physics::collision_component>()->bounding_boxes[0].collision_information.colliding ? "true" : "false");
            ImGui::End();
            });
    }
private:
	gse::vec3<gse::length> m_initial_position;
	gse::vec3<gse::length> m_size;
};

gse::box::box(const vec3<length>& initial_position, const vec3<length>& size) {
	add_hook(std::make_unique<box_mesh_hook>(this, initial_position, size));
}
