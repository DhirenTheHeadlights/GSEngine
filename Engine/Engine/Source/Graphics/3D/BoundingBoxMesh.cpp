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
	constexpr float cell_size = 10.0f;

	m_vertices.clear();

	const glm::vec3 min = m_lower.as<units::meters>();
	const glm::vec3 max = m_upper.as<units::meters>();

	// Generate grid lines for each face
	for (float y = min.y; y <= max.y; y += cell_size) {
		for (float x = min.x; x <= max.x; x += cell_size) {
			// Vertical lines (front and back faces)
			m_vertices.push_back(create_vertex({ x, y, min.z }));
			m_vertices.push_back(create_vertex({ x, y, max.z }));

			// Horizontal lines along x-axis (front and back faces)
			if (x + cell_size <= max.x) {
				float x_end = x + cell_size;
				m_vertices.push_back(create_vertex({ x, y, min.z }));
				m_vertices.push_back(create_vertex({ x_end, y, min.z }));

				m_vertices.push_back(create_vertex({ x, y, max.z }));
				m_vertices.push_back(create_vertex({ x_end, y, max.z }));
			}

			// Horizontal lines along z-axis on left and right faces
			if (std::abs(x - max.x) < 1e-5 || std::abs(x - min.x) < 1e-5) {
				for (float z = min.z; z + cell_size <= max.z; z += cell_size) {
					float z_end = z + cell_size;
					m_vertices.push_back(create_vertex({ x, y, z }));
					m_vertices.push_back(create_vertex({ x, y, z_end }));
				}
			}

			// Vertical lines along y-axis on left and right faces
			if (y + cell_size <= max.y) {
				float y_end = y + cell_size;
				for (float z = min.z; z <= max.z; z += cell_size) {
					m_vertices.push_back(create_vertex({ x, y, z }));
					m_vertices.push_back(create_vertex({ x, y_end, z }));
				}
			}
		}
	}

	// Top and bottom faces grid lines
	for (float z = min.z; z <= max.z; z += cell_size) {
		for (float x = min.x; x <= max.x; x += cell_size) {
			if (x + cell_size <= max.x) {
				float x_end = x + cell_size;
				m_vertices.push_back(create_vertex({ x, max.y, z }));
				m_vertices.push_back(create_vertex({ x_end, max.y, z }));

				m_vertices.push_back(create_vertex({ x, min.y, z }));
				m_vertices.push_back(create_vertex({ x_end, min.y, z }));
			}

			if (z + cell_size <= max.z) {
				float z_end = z + cell_size;
				m_vertices.push_back(create_vertex({ x, max.y, z }));
				m_vertices.push_back(create_vertex({ x, max.y, z_end }));

				m_vertices.push_back(create_vertex({ x, min.y, z }));
				m_vertices.push_back(create_vertex({ x, min.y, z_end }));
			}
		}
	}
}

void gse::bounding_box_mesh::update() {
	update_grid();
	set_up_mesh();
}
