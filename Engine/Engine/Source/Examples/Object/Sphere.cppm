export module gse.examples:sphere;

import std;

import gse.runtime;
import gse.utility;
import gse.graphics;
import gse.physics;

import :procedural_models;

export namespace gse {
	class sphere final : public hook<entity> {
	public:
		struct params {
			vec3<length> initial_position = vec3<length>(0.f, 0.f, 0.f);
			length radius = meters(1.f);
			int sectors = 36;
			int stacks = 18;
		};

		explicit sphere(const params& p) : m_params(p) {
		}

		auto initialize() -> void override {
			add_component<physics::motion_component>({
				.current_position = m_params.initial_position,
				.mass = kilograms(100.f)
			});

			const vec3<length> size(
				m_params.radius * 2.f,
				m_params.radius * 2.f,
				m_params.radius * 2.f
			);

			add_component<physics::collision_component>({
				.bounding_box = { m_params.initial_position, size }
			});

			add_component<render_component>({
				.models = {
					procedural_model::sphere(
						gse::queue<material>(
							"sun_material",
							gse::get<texture>("Textures/Textures/sun")
						),
						m_params.sectors,
						m_params.stacks
					)
				}
			});
		}
	private:
		params m_params;
	};
}
