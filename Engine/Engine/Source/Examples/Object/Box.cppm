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
			const auto s = m_size.as<meters>();
			const float mass_val = m_box_mass.as<kilograms>();
			const float box_inertia = (mass_val / 18.f) * (s.x() * s.x() + s.y() * s.y() + s.z() * s.z());

			add_component<physics::motion_component>({
				.current_position = m_initial_position,
				.mass = m_box_mass,
				.orientation = m_initial_orientation,
				.moment_of_inertia = kilograms_meters_squared(box_inertia)
			});

			add_component<physics::collision_component>({
				.bounding_box = { m_initial_position, m_size }
			});

			static std::mt19937 rng(std::random_device{}());
			static std::uniform_real_distribution<float> dist(0.3f, 1.0f);
			static int box_color_id = 0;
			const auto id_str = std::to_string(box_color_id++);

			const auto tex = gse::queue<texture>(
				"box_color_" + id_str,
				unitless::vec4(dist(rng), dist(rng), dist(rng), 1.0f)
			);

			const auto mat = gse::queue<material>(
				"box_mat_" + id_str,
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
