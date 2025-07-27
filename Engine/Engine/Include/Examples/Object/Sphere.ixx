export module gse.examples:sphere;

import std;

import gse.runtime;
import gse.utility;
import gse.graphics;
import gse.physics;

export namespace gse {
	class sphere final : public hook<entity> {
	public:
		struct params {
			vec3<length> initial_position = vec3<length>(0.f, 0.f, 0.f);
			length radius = meters(1.f);
			int sectors = 36; // Number of slices around the sphere
			int stacks = 18;  // Number of stacks from top to bottom
		};

		sphere(const params& p) : m_params(p) {}

		auto initialize() -> void override {
			const auto [mc_id, mc] = add_component<physics::motion_component>({
				.current_position = m_params.initial_position
			});

			const float r = m_params.radius.as<units::meters>();

			std::vector<vertex> vertices;
			std::vector<std::uint32_t> indices;

			// Generate vertices
			for (int stack = 0; stack <= m_params.stacks; ++stack) {
				const float phi = std::numbers::pi_v<float> *(static_cast<float>(stack) / static_cast<float>(m_params.stacks)); // From 0 to PI
				const float sin_phi = std::sin(phi);
				const float cos_phi = std::cos(phi);

				for (int sector = 0; sector <= m_params.sectors; ++sector) {
					const float theta = 2 * std::numbers::pi_v<float> *(static_cast<float>(sector) / static_cast<float>(m_params.sectors)); // From 0 to 2PI
					const float sin_theta = std::sin(theta);
					const float cos_theta = std::cos(theta);

					// Calculate vertex position
					vec3<length> position = {
						r * sin_phi * cos_theta,
						r * cos_phi,
						r * sin_phi * sin_theta
					};

					// Calculate normal (normalized position for a sphere)
					const unitless::vec3 normal = normalize(position);

					// Calculate texture coordinates
					const unitless::vec2 tex_coords = {
						static_cast<float>(sector) / static_cast<float>(m_params.sectors),
						static_cast<float>(stack) / static_cast<float>(m_params.stacks)
					};

					vertices.push_back({ .position = position.as<units::meters>().storage, .normal = normal.storage, .tex_coords = tex_coords.storage });
				}
			}

			// Generate indices
			for (int stack = 0; stack < m_params.stacks; ++stack) {
				for (int sector = 0; sector < m_params.sectors; ++sector) {
					const int current = stack * (m_params.sectors + 1) + sector;
					const int next = current + m_params.sectors + 1;

					// Two triangles per quad
					indices.push_back(current);
					indices.push_back(next);
					indices.push_back(current + 1);

					indices.push_back(current + 1);
					indices.push_back(next);
					indices.push_back(next + 1);
				}
			}

			std::vector<mesh_data> new_meshes;
			new_meshes.emplace_back(
				vertices,
				indices,
				gse::queue<material>(
					"sun_material",
					gse::get<texture>("sun")
				)
			);

			add_component<render_component>({ 
				.models = { gse::queue<model>("Sphere", new_meshes) }
			});
		}
	private:
		params m_params;
	};
}