module;

#include <glad/glad.h>

export module gse.graphics.bounding_box_mesh;

import std;
import glm;

import gse.graphics.mesh;
import gse.physics.math;
import gse.physics.bounding_box;

export namespace gse {
	class bounding_box_mesh final : public mesh {
	public:
		bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper);
		bounding_box_mesh(const axis_aligned_bounding_box& box);

		auto update() -> void;

		auto set_cell_size(const length& cell_size) -> void { m_cell_size = cell_size; }
	private:
		auto update_grid() -> void;
		static auto create_vertex(const glm::vec3& position) -> vertex;

		vec3<length> m_lower;
		vec3<length> m_upper;

		length m_cell_size = meters(10.0f);
	};
}

gse::bounding_box_mesh::bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper)
	: m_lower(lower), m_upper(upper) {
	update_grid();
}

gse::bounding_box_mesh::bounding_box_mesh(const axis_aligned_bounding_box& box)
	: bounding_box_mesh(box.lower_bound, box.upper_bound) {
}

auto gse::bounding_box_mesh::create_vertex(const glm::vec3& position) -> vertex {
	return {
		.position = position,
		.normal = glm::vec3(0.0f),
		.tex_coords = glm::vec2(0.0f)
	};
}

auto gse::bounding_box_mesh::update_grid() -> void {
	constexpr float cell_size = 10.0f;

	vertices.clear();

	const glm::vec3 min = m_lower.as<units::meters>();
	const glm::vec3 max = m_upper.as<units::meters>();

	// Generate grid lines for each face
	for (float y = min.y; y <= max.y; y += cell_size) {
		for (float x = min.x; x <= max.x; x += cell_size) {
			// Vertical lines (front and back faces)
			vertices.push_back(create_vertex({ x, y, min.z }));
			vertices.push_back(create_vertex({ x, y, max.z }));

			// Horizontal lines along x-axis (front and back faces)
			if (x + cell_size <= max.x) {
				float x_end = x + cell_size;
				vertices.push_back(create_vertex({ x, y, min.z }));
				vertices.push_back(create_vertex({ x_end, y, min.z }));

				vertices.push_back(create_vertex({ x, y, max.z }));
				vertices.push_back(create_vertex({ x_end, y, max.z }));
			}

			// Horizontal lines along z-axis on left and right faces
			if (std::abs(x - max.x) < 1e-5 || std::abs(x - min.x) < 1e-5) {
				for (float z = min.z; z + cell_size <= max.z; z += cell_size) {
					float z_end = z + cell_size;
					vertices.push_back(create_vertex({ x, y, z }));
					vertices.push_back(create_vertex({ x, y, z_end }));
				}
			}

			// Vertical lines along y-axis on left and right faces
			if (y + cell_size <= max.y) {
				float y_end = y + cell_size;
				for (float z = min.z; z <= max.z; z += cell_size) {
					vertices.push_back(create_vertex({ x, y, z }));
					vertices.push_back(create_vertex({ x, y_end, z }));
				}
			}
		}
	}

	// Top and bottom faces grid lines
	for (float z = min.z; z <= max.z; z += cell_size) {
		for (float x = min.x; x <= max.x; x += cell_size) {
			if (x + cell_size <= max.x) {
				float x_end = x + cell_size;
				vertices.push_back(create_vertex({ x, max.y, z }));
				vertices.push_back(create_vertex({ x_end, max.y, z }));

				vertices.push_back(create_vertex({ x, min.y, z }));
				vertices.push_back(create_vertex({ x_end, min.y, z }));
			}

			if (z + cell_size <= max.z) {
				float z_end = z + cell_size;
				vertices.push_back(create_vertex({ x, max.y, z }));
				vertices.push_back(create_vertex({ x, max.y, z_end }));

				vertices.push_back(create_vertex({ x, min.y, z }));
				vertices.push_back(create_vertex({ x, min.y, z_end }));
			}
		}
	}
}

auto gse::bounding_box_mesh::update() -> void {
	/*update_grid();
	set_up_mesh();*/
}