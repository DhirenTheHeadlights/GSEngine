export module gse.examples:box;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.graphics;

import :procedural_models;

export namespace gse {
	class box final : public hook<entity> {
	public:
		struct params {
			vec3<position> initial_position = vec3<position>(0.f, 0.f, 0.f);
			vec3<length> size = vec3<length>(1.f, 1.f, 1.f);
			quat initial_orientation = quat(1.f, 0.f, 0.f, 0.f);
			mass mass = kilograms(1000.f);
			float roughness = 0.5f;
			float metallic = 0.0f;
		};

		explicit box(const params& p) : m_initial_position(p.initial_position), m_size(p.size), m_initial_orientation(p.initial_orientation), m_box_mass(p.mass), m_roughness(p.roughness), m_metallic(p.metallic) {}

		auto initialize() -> void override {
			static constexpr density row = kilograms_per_cubic_meter(2000.f);
			const inertia box_inertia = m_box_mass * dot(m_size, m_size) / 18.f;

			add_component<physics::motion_component>({
				.current_position = m_initial_position,
				.mass = m_box_mass,
				.orientation = m_initial_orientation,
				.moment_of_inertia = box_inertia
			});

			add_component<physics::collision_component>({
				.bounding_box = { m_initial_position, m_size }
			});

			const material mat{
				.base_color = vec3f(random_value(0.3f, 1.0f), random_value(0.3f, 1.0f), random_value(0.3f, 1.0f)),
				.roughness = m_roughness,
				.metallic = m_metallic,
			};

			add_component<render_component>({
				.models = { procedural_model::box(mat, m_size) }
			});
		}
	private:
		vec3<position> m_initial_position;
		vec3<length> m_size;
		quat m_initial_orientation;
		mass m_box_mass;
		float m_roughness;
		float m_metallic;
	};
}
