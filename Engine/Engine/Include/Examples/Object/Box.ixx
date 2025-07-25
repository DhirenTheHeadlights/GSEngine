module;

#include <imgui.h>

export module gse.examples:box;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.graphics;

export namespace gse {
    class box final : public hook<entity> {
    public:
        struct params {
	        vec3<length> initial_position = vec3<length>(0.f, 0.f, 0.f);
			vec3<length> size = vec3<length>(1.f, 1.f, 1.f);
        };

        box(const id& owner_id, registry* reg, const params& p) : hook(owner_id, reg), m_initial_position(p.initial_position), m_size(p.size) {}

        auto initialize() -> void override {
            const auto [mc_id, mc] = add_component<physics::motion_component>({
                .current_position = m_initial_position,
                .mass = kilograms(100.f)
            });

            const auto [cc_id, cc] = add_component<physics::collision_component>({
				.bounding_box = { m_initial_position, m_size }
            });
			cc->oriented_bounding_box = { cc->bounding_box };

            const float half_width = m_size.as<units::meters>().x / 2.f;
            const float half_height = m_size.as<units::meters>().y / 2.f;
            const float half_depth = m_size.as<units::meters>().z / 2.f;

            const unitless::vec3 color = { random_value(0.f, 1.f), random_value(0.f, 1.f), random_value(0.f, 1.f) };

            const float repeat_interval = 10.f; // 1 repeat per unit (meters in this case)

            const std::vector<std::vector<vertex>> face_vertices = {
                {
                    { { -half_width, -half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { 0.0f,              0.0f } },
                    { {  half_width, -half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { repeat_interval,   0.0f } },
                    { {  half_width,  half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { repeat_interval,   repeat_interval } },
                    { { -half_width,  half_height, -half_depth }, {  0.0f,  0.0f, -1.0f }, { 0.0f,              repeat_interval } }
                },

                {
                    { { -half_width, -half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { 0.0f,              0.0f } },
                    { { -half_width,  half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { 0.0f,              repeat_interval } },
                    { {  half_width,  half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { repeat_interval,   repeat_interval } },
                    { {  half_width, -half_height,  half_depth }, {  0.0f,  0.0f,  1.0f }, { repeat_interval,   0.0f } }
                },

                {
                    { { -half_width, -half_height,  half_depth }, { -1.0f,  0.0f,  0.0f }, { 0.0f,              0.0f } },
                    { { -half_width, -half_height, -half_depth }, { -1.0f,  0.0f,  0.0f }, { repeat_interval,   0.0f } },
                    { { -half_width,  half_height, -half_depth }, { -1.0f,  0.0f,  0.0f }, { repeat_interval,   repeat_interval } },
                    { { -half_width,  half_height,  half_depth }, { -1.0f,  0.0f,  0.0f }, { 0.0f,              repeat_interval } }
                },

                {
                    { {  half_width, -half_height, -half_depth }, {  1.0f,  0.0f,  0.0f }, { 0.0f,              0.0f } },
                    { {  half_width, -half_height,  half_depth }, {  1.0f,  0.0f,  0.0f }, { repeat_interval,   0.0f } },
                    { {  half_width,  half_height,  half_depth }, {  1.0f,  0.0f,  0.0f }, { repeat_interval,   repeat_interval } },
                    { {  half_width,  half_height, -half_depth }, {  1.0f,  0.0f,  0.0f }, { 0.0f,              repeat_interval } }
                },

                {
                    { { -half_width,  half_height, -half_depth }, {  0.0f,  1.0f,  0.0f }, { 0.0f,              0.0f } },
                    { { -half_width,  half_height,  half_depth }, {  0.0f,  1.0f,  0.0f }, { 0.0f,              repeat_interval } },
                    { {  half_width,  half_height,  half_depth }, {  0.0f,  1.0f,  0.0f }, { repeat_interval,   repeat_interval } },
                    { {  half_width,  half_height, -half_depth }, {  0.0f,  1.0f,  0.0f }, { repeat_interval,   0.0f } }
                },

                {
                    { { -half_width, -half_height,  half_depth }, {  0.0f, -1.0f,  0.0f }, { 0.0f,              0.0f } },
                    { { -half_width, -half_height, -half_depth }, {  0.0f, -1.0f,  0.0f }, { 0.0f,              repeat_interval } },
                    { {  half_width, -half_height, -half_depth }, {  0.0f, -1.0f,  0.0f }, { repeat_interval,   repeat_interval } },
                    { {  half_width, -half_height,  half_depth }, {  0.0f, -1.0f,  0.0f }, { repeat_interval,   0.0f } }
                }
            };

            std::vector<std::uint32_t> face_indices = { 0, 1, 2, 2, 3, 0 };

            std::vector<mesh_data> meshes;

            for (size_t i = 0; i < 6; ++i) {
                meshes.emplace_back(
                    face_vertices[i],
                    face_indices,
                    gse::queue<material>(
                        "concrete_bricks_material",
                        gse::get<texture>("concrete_bricks_1")
                    )
                );
            }

            add_component<render_component>({
                .models = { gse::queue<model>("Box", meshes) }
            });
        }

        auto render() -> void override {
            debug::add_imgui_callback(
                [this] {
	                auto& render_component = component<gse::render_component>();
	                ImGui::Begin(owner_id().tag().c_str());
	                ImGui::SliderFloat3("Position", &component<physics::motion_component>().current_position.x.as_default_unit(), -1000.f, 1000.f);
	                ImGui::Text("Colliding: %s", component<physics::collision_component>().collision_information.colliding ? "true" : "false");
	                ImGui::Text("Center of Mass: %f, %f, %f", render_component.center_of_mass.x.as_default_unit(), render_component.center_of_mass.y.as_default_unit(), render_component.center_of_mass.z.as_default_unit());
	                ImGui::End();
                }
            );
        }
    private:
        vec3<length> m_initial_position;
        vec3<length> m_size;
    };
}
