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
			quat initial_orientation = quat(1.f, 0.f, 0.f, 0.f);
			mass box_mass = kilograms(1000.f);
		};

		explicit box(const params& p) : m_initial_position(p.initial_position), m_size(p.size), m_initial_orientation(p.initial_orientation), m_box_mass(p.box_mass) {}

		auto initialize() -> void override {
			const auto s = m_size.as<meters>();
			const float mass_val = m_box_mass.as<kilograms>();
			const float box_inertia = (mass_val / 18.f) * (s.x() * s.x() + s.y() * s.y() + s.z() * s.z());

			add_component<physics::motion_component>({
				.current_position = m_initial_position,
				.mass = m_box_mass,
				.orientation = m_initial_orientation,
				.moment_of_inertia = box_inertia
			});

			add_component<physics::collision_component>({
				.bounding_box = { m_initial_position, m_size }
			});

			const auto mat = gse::queue<material>(
				"concrete_bricks_material",
				gse::get<texture>("Textures/Textures/concrete_bricks_1")
			);

			add_component<render_component>({
				.models = { procedural_model::box(mat, m_size) }
			});

			actions::bind_button_channel<"Box_Rotate">(owner_id(), key::r, m_rotate_channel);
		}

		auto update() -> void override {
			auto& motion = component_write<physics::motion_component>();

			if (!motion.affected_by_gravity) {
				return;
			}

			if (!m_rotate_channel.pressed) {
				return;
			}

			motion.orientation += quat(unitless::axis_z, degrees(26.f));
		}

		auto render() -> void override {
			gui::start(
				std::format("Box {}", owner_id()),
				[&motion = component_write<physics::motion_component>()] {
					gui::slider("Position", motion.current_position, vec3<length>(-100.f), vec3<length>(100.f));
				}
			);
		}
	private:
		vec3<length> m_initial_position;
		vec3<length> m_size;
		quat m_initial_orientation;
		mass m_box_mass;

		actions::button_channel m_rotate_channel;
	};
}
