#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Graphics/RenderQueueEntry.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	struct vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 tex_coords;
	};

	class render_component;

	class mesh {
	public:
		mesh();
		mesh(const std::vector<vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3& color = { 1.0f, 1.0f, 1.0f }, GLuint texture_id = 0);
		virtual ~mesh();

		// Disable copy to avoid OpenGL resource duplication
		mesh(const mesh&) = delete;
		mesh& operator=(const mesh&) = delete;

		// Allow move semantics
		mesh(mesh&& other) noexcept;
		mesh& operator=(mesh&& other) noexcept;

		GLuint get_vao() const;
		GLsizei get_vertex_count() const { return static_cast<GLsizei>(m_indices.size()); }

		virtual render_queue_entry get_queue_entry() const {
			return {
				m_material_name,
				m_vao,
				m_draw_mode,
				get_vertex_count(),
				m_model_matrix,
				m_color
			};
		}

		void set_color(const glm::vec3& new_color) { m_color = new_color; }
		void set_position(const vec3<length>& new_position);

	protected:
		friend render_component;
		void set_up_mesh();

		GLuint m_vao = 0;
		GLuint m_vbo = 0;
		GLuint m_ebo = 0;

		std::vector<vertex> m_vertices;
		std::vector<unsigned int> m_indices;

		glm::mat4 m_model_matrix = glm::mat4(1.0f);
		GLuint m_draw_mode = GL_TRIANGLES;
		std::string m_material_name = "Concrete";
		glm::vec3 m_color = { 1.0f, 1.0f, 1.0f };
	};
}
