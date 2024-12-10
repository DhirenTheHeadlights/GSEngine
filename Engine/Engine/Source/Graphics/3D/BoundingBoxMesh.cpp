#include "Graphics/3D/BoundingBoxMesh.h"

gse::bounding_box_mesh::bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper)
	: m_lower(lower), m_upper(upper) {
	update_grid();
	m_draw_mode = GL_LINES;
	m_shader_name = "SolidColor";
}

gse::vertex gse::bounding_box_mesh::create_vertex(const glm::vec3& position) {
	return {
		position,
		glm::vec3(0.0f),
		glm::vec2(0.0f)
	};
}

void gse::bounding_box_mesh::update_grid() {
	constexpr float cellSize = 10.0f;

	m_vertices.clear();

	const glm::vec3 min = m_lower.as<Meters>();
	const glm::vec3 max = m_upper.as<Meters>();

	// Generate grid lines for each face
	for (float y = min.y; y <= max.y; y += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			// Vertical lines (front and back faces)
			m_vertices.push_back(create_vertex({ x, y, min.z }));
			m_vertices.push_back(create_vertex({ x, y, max.z }));

			// Horizontal lines along x-axis (front and back faces)
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				m_vertices.push_back(create_vertex({ x, y, min.z }));
				m_vertices.push_back(create_vertex({ xEnd, y, min.z }));

				m_vertices.push_back(create_vertex({ x, y, max.z }));
				m_vertices.push_back(create_vertex({ xEnd, y, max.z }));
			}

			// Horizontal lines along z-axis on left and right faces
			if (std::abs(x - max.x) < 1e-5 || std::abs(x - min.x) < 1e-5) {
				for (float z = min.z; z + cellSize <= max.z; z += cellSize) {
					float zEnd = z + cellSize;
					m_vertices.push_back(create_vertex({ x, y, z }));
					m_vertices.push_back(create_vertex({ x, y, zEnd }));
				}
			}

			// Vertical lines along y-axis on left and right faces
			if (y + cellSize <= max.y) {
				float yEnd = y + cellSize;
				for (float z = min.z; z <= max.z; z += cellSize) {
					m_vertices.push_back(create_vertex({ x, y, z }));
					m_vertices.push_back(create_vertex({ x, yEnd, z }));
				}
			}
		}
	}

	// Top and bottom faces grid lines
	for (float z = min.z; z <= max.z; z += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				m_vertices.push_back(create_vertex({ x, max.y, z }));
				m_vertices.push_back(create_vertex({ xEnd, max.y, z }));

				m_vertices.push_back(create_vertex({ x, min.y, z }));
				m_vertices.push_back(create_vertex({ xEnd, min.y, z }));
			}

			if (z + cellSize <= max.z) {
				float zEnd = z + cellSize;
				m_vertices.push_back(create_vertex({ x, max.y, z }));
				m_vertices.push_back(create_vertex({ x, max.y, zEnd }));

				m_vertices.push_back(create_vertex({ x, min.y, z }));
				m_vertices.push_back(create_vertex({ x, min.y, zEnd }));
			}
		}
	}
}

void gse::bounding_box_mesh::update() {
	update_grid();
	set_up_mesh();
}
