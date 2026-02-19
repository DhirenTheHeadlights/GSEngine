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
			mass mass = kilograms(1000.f);
		};

		explicit box(const params& p) : m_initial_position(p.initial_position), m_size(p.size), m_initial_orientation(p.initial_orientation), m_box_mass(p.mass) {}

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
		}
	private:
		vec3<length> m_initial_position;
		vec3<length> m_size;
		quat m_initial_orientation;
		mass m_box_mass;
	};
}
