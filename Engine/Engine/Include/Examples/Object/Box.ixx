module;

#include <imgui.h>

export module gse.examples:box;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.graphics;

export class box_mesh final : public gse::hook<gse::entity> {
public:
    box_mesh(const gse::id& owner_id, gse::scene* scene, const gse::vec3<gse::length>& initial_position, const gse::vec3<gse::length>& size) : hook(owner_id, scene), m_initial_position(initial_position), m_size(size) {}

    auto initialize() -> void override {
		const auto mc = m_scene->registry().add_link<gse::physics::motion_component>(owner_id());
        mc->current_position = m_initial_position;
        mc->mass = gse::kilograms(100.f);

		const auto cc = m_scene->registry().add_link<gse::physics::collision_component>(owner_id());
        cc->bounding_box = { m_initial_position, m_size };
        cc->oriented_bounding_box = { cc->bounding_box };

        const float half_width = m_size.as<gse::units::meters>().x / 2.f;
        const float half_height = m_size.as<gse::units::meters>().y / 2.f;
        const float half_depth = m_size.as<gse::units::meters>().z / 2.f;

        const gse::unitless::vec3 color = { gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f), gse::random_value(0.f, 1.f) };

        const float repeat_interval = 10.f; // 1 repeat per unit (meters in this case)

        const std::vector<std::vector<gse::vertex>> face_vertices = {
            {
                { { -half_width, -half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { 0.0f,          0.0f } },
                { { -half_width,  half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { 0.0f,  repeat_interval } },
                { {  half_width,  half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { repeat_interval, repeat_interval } },
                { {  half_width, -half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { repeat_interval, 0.0f } }
            },

            {
                { { -half_width, -half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { 0.0f,          0.0f } },
                { {  half_width, -half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { repeat_interval, 0.0f } },
                { {  half_width,  half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { repeat_interval, repeat_interval } },
                { { -half_width,  half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { 0.0f,  repeat_interval } }
            },

            {
                { { -half_width, -half_height,  half_depth }, { -1.0f,  0.0f,  0.0f }, { 0.0f,          0.0f } },
                { { -half_width,  half_height,  half_depth }, { -1.0f,  0.0f,  0.0f }, { 0.0f,  repeat_interval } },
                { { -half_width,  half_height, -half_depth }, { -1.0f,  0.0f,  0.0f }, { repeat_interval, repeat_interval } },
                { { -half_width, -half_height, -half_depth }, { -1.0f,  0.0f,  0.0f }, { repeat_interval, 0.0f } }
            },

            {
                { {  half_width, -half_height, -half_depth }, {  1.0f,  0.0f,  0.0f }, { 0.0f,          0.0f } },
                { {  half_width,  half_height, -half_depth }, {  1.0f,  0.0f,  0.0f }, { 0.0f,  repeat_interval } },
                { {  half_width,  half_height,  half_depth }, {  1.0f,  0.0f,  0.0f }, { repeat_interval, repeat_interval } },
                { {  half_width, -half_height,  half_depth }, {  1.0f,  0.0f,  0.0f }, { repeat_interval, 0.0f } }
            },

            {
                { { -half_width,  half_height, -half_depth }, {  0.0f,  1.0f,  0.0f }, { 0.0f,          0.0f } },
                { { -half_width,  half_height,  half_depth }, {  0.0f,  1.0f,  0.0f }, { 0.0f,  repeat_interval } },
                { {  half_width,  half_height,  half_depth }, {  0.0f,  1.0f,  0.0f }, { repeat_interval, repeat_interval } },
                { {  half_width,  half_height, -half_depth }, {  0.0f,  1.0f,  0.0f }, { repeat_interval, 0.0f } }
            },

            {
                { { -half_width, -half_height,  half_depth }, {  0.0f, -1.0f,  0.0f }, { 0.0f,          0.0f } },
                { { -half_width, -half_height, -half_depth }, {  0.0f, -1.0f,  0.0f }, { 0.0f,  repeat_interval } },
                { {  half_width, -half_height, -half_depth }, {  0.0f, -1.0f,  0.0f }, { repeat_interval, repeat_interval } },
                { {  half_width, -half_height,  half_depth }, {  0.0f, -1.0f,  0.0f }, { repeat_interval, 0.0f } }
            }
        };


        std::vector<std::uint32_t> face_indices = { 0, 1, 2, 2, 3, 0 };

        std::vector<gse::mesh_data> meshes;

        for (size_t i = 0; i < 6; ++i) {
            meshes.emplace_back(
                face_vertices[i], 
                face_indices, 
                gse::queue<gse::material>(
                    "concrete_bricks_material", 
                    gse::queue<gse::texture>(gse::config::resource_path / "Textures/concrete_bricks_1.jpg", "concrete_bricks")
                )
            );
        }

		m_scene->registry().add_link<gse::render_component>(owner_id(), gse::queue<gse::model>("Box", std::move(meshes)));
    }

    auto update() -> void override {
        component<gse::render_component>().models[0].set_position(component<gse::physics::motion_component>().current_position);
        component<gse::physics::collision_component>().bounding_box.set_position(component<gse::physics::motion_component>().current_position);
    }

    auto render() -> void override {
        gse::debug::add_imgui_callback([this] {
            auto& render_component = component<gse::render_component>();
            ImGui::Begin(owner_id().tag().c_str());
            ImGui::SliderFloat3("Position", &component<gse::physics::motion_component>().current_position.x.as_default_unit(), -1000.f, 1000.f);
            ImGui::Text("Colliding: %s", component<gse::physics::collision_component>().collision_information.colliding ? "true" : "false");
            ImGui::Text("Center of Mass: %f, %f, %f", render_component.center_of_mass.x.as_default_unit(), render_component.center_of_mass.y.as_default_unit(), render_component.center_of_mass.z.as_default_unit());
            ImGui::End();
            });
    }
private:
    gse::vec3<gse::length> m_initial_position;
    gse::vec3<gse::length> m_size;
};
