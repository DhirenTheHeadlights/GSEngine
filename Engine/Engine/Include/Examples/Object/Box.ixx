export module gse.examples:box;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.graphics;
import gse.platform;

import :procedural_models;

export namespace gse {
    class box final : public hook<entity> {
    public:
        struct params {
            vec3<length> initial_position = vec3<length>(0.f, 0.f, 0.f);
            vec3<length> size = vec3<length>(1.f, 1.f, 1.f);
        };

        box(const params& p) : m_initial_position(p.initial_position), m_size(p.size) {}

        auto initialize() -> void override {
            add_component<physics::motion_component>({
                .current_position = m_initial_position,
                .mass = kilograms(100.f)
            });

            const auto [cc_id, cc] = add_component<physics::collision_component>({
                .bounding_box = { m_initial_position, m_size }
            });

            const auto mat = gse::queue<material>(
                "concrete_bricks_material",
                gse::get<texture>("concrete_bricks_1")
            );

            add_component<render_component>({
                .models = { procedural_model::box(mat) }
            });
        }

        auto update() -> void override {
            if (keyboard::pressed(key::r) && component<physics::motion_component>().affected_by_gravity) {
				component<physics::motion_component>().orientation = quat(unitless::axis_z, degrees(35));
                component<physics::motion_component>().orientation = quat(unitless::axis_x, degrees(75));
            }
        }
    private:
        vec3<length> m_initial_position;
        vec3<length> m_size;
    };
}
