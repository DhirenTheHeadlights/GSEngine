export module gse.graphics.bounding_box_mesh;

import std;
import glm;

import gse.graphics.mesh;
import gse.physics.math.vector;
import gse.physics.bounding_box;

export namespace gse {
	class bounding_box_mesh final : public mesh {
	public:
		bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper);
		bounding_box_mesh(const axis_aligned_bounding_box& box);

		auto update() -> void;

		auto get_queue_entry() const -> render_queue_entry override {
			return {
				.material_key	= m_material_name,
				.vao			= m_vao,
				.draw_mode		= m_draw_mode,
				.vertex_count	= static_cast<GLsizei>(m_vertices.size() / 3),
				.model_matrix	= m_model_matrix,
				.color			= m_color
			};
		}

		auto set_cell_size(const length& cell_size) -> void { m_cell_size = cell_size; }

	private:
		auto update_grid() -> void;
		static auto create_vertex(const glm::vec3& position) -> vertex;

		vec3<length> m_lower;
		vec3<length> m_upper;

		length m_cell_size = meters(10.0f);
	};
}