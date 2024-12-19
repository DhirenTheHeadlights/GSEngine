#include "Core/Object/Box.h"
#include <imgui.h>
#include "Graphics/RenderComponent.h"

void gse::box::initialize() {
    const auto new_render_component = std::make_shared<render_component>(m_id.get());

    const float half_width = m_size.as<units::meters>().x / 2.f;
    const float half_height = m_size.as<units::meters>().y / 2.f;
    const float half_depth = m_size.as<units::meters>().z / 2.f;

    const glm::vec3 pos_offset = m_position.as<units::meters>();

    const glm::vec3 colors[] = {
        {.75f, 0.5f, 0.2f},  // Front
        {0.2f, 1.0f, 0.3f},  // Back
        {0.3f, 1.0f, 0.8f},  // Left
        {1.0f, 1.0f, 1.0f},  // Right
        {0.3f, 0.3f, 0.8f},  // Top
        {0.2f, 1.0f, 0.3f}   // Bottom
    };

    const std::vector<std::vector<vertex>> face_vertices = {
        // Front face
        {
            { glm::vec3(-half_width, -half_height, half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Back face
        {
            { glm::vec3(-half_width, -half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Left face
        {
            { glm::vec3(-half_width, -half_height, -half_depth) + pos_offset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(-half_width, -half_height, half_depth) + pos_offset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(-half_width, half_height, half_depth) + pos_offset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth) + pos_offset, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Right face
        {
            { glm::vec3(half_width, -half_height, -half_depth) + pos_offset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth) + pos_offset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth) + pos_offset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(half_width, half_height, -half_depth) + pos_offset, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Top face
        {
            { glm::vec3(-half_width, half_height, half_depth) + pos_offset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, half_height, half_depth) + pos_offset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, half_height, -half_depth) + pos_offset, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        },
        // Bottom face
        {
            { glm::vec3(-half_width, -half_height, half_depth) + pos_offset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, half_depth) + pos_offset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3(half_width, -half_height, -half_depth) + pos_offset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
            { glm::vec3(-half_width, -half_height, -half_depth) + pos_offset, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
        }
    };

    const std::vector<unsigned int> face_indices = { 0, 1, 2, 2, 3, 0 };

    for (size_t i = 0; i < 6; ++i) {
	    const auto new_mesh = std::make_shared<mesh>(face_vertices[i], face_indices);
        new_mesh->set_color(colors[i]);
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
private:
	gse::vec3<gse::length> m_initial_position;
	gse::vec3<gse::length> m_size;
};

gse::box::box(const vec3<length>& initial_position, const vec3<length>& size) : object("Box") {
	add_hook(std::make_unique<box_mesh_hook>(this, initial_position, size));
}
private:
	gse::vec3<gse::length> m_initial_position;
	gse::vec3<gse::length> m_size;
};

gse::box::box(const vec3<length>& initial_position, const vec3<length>& size) : object("Box") {
	add_hook(std::make_unique<box_mesh_hook>(this, initial_position, size));
}
