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

			const auto name = std::string(owner_id().tag());

			const auto tex = gse::queue<texture>(
				name + "_color",
				vec4f(random_value(0.3f, 1.0f), random_value(0.3f, 1.0f), random_value(0.3f, 1.0f), 1.0f)
			);

			const auto mat = gse::queue<material>(
				name + "_mat",
				tex
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
