#pragma once
#include <vector>

#include "BoundingBox.h"
#include "Graphics/3D/Mesh.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	class bounding_box_mesh final : public mesh {
	public:
		bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper);
		bounding_box_mesh(const bounding_box& box);

		void update();

		render_queue_entry get_queue_entry() const override {
			return {
				m_material_name,
				m_vao,
				m_draw_mode,
				static_cast<GLsizei>(m_vertices.size() / 3),
				m_model_matrix,
				m_color
			};
		}

		void set_cell_size(const length& cell_size) { m_cell_size = cell_size; }

	private:
		void update_grid();
		static vertex create_vertex(const glm::vec3& position);

		vec3<length> m_lower;
		vec3<length> m_upper;

		length m_cell_size = meters(10.0f);
	};
}