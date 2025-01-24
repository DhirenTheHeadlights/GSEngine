module;

#include <glad/glad.h>

export module gse.graphics.mesh;

import std;
import glm;

import gse.physics.math.vector;

export namespace gse {
	struct vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 tex_coords;
	};

	struct render_queue_entry {
		std::string material_key;
		GLuint vao;
		GLenum draw_mode;
		GLsizei vertex_count;
		glm::mat4 model_matrix;
		glm::vec3 color;
	};

	struct model_texture {
		unsigned int id;
		std::string type;
		std::string path;
	};

	struct render_component;

	class mesh {
	public:
		mesh();
		mesh(const std::vector<vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3& color = { 1.0f, 1.0f, 1.0f }, GLuint texture_id = 0);
		mesh(const std::vector<vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<model_texture>& textures);
		virtual ~mesh();

		mesh(const mesh&) = delete;
		auto operator=(const mesh&) -> mesh& = delete;

		mesh(mesh&& other) noexcept;
		auto operator=(mesh&& other) noexcept -> mesh&;

		auto get_vao() const -> GLuint;
		auto get_vertex_count() const -> GLsizei ;
		auto calculate_center_of_mass() -> vec3<length>;

		virtual auto get_queue_entry() const -> render_queue_entry {
			return {
				.material_key	= m_material_name,
				.vao			= m_vao,
				.draw_mode		= m_draw_mode,
				.vertex_count	= get_vertex_count(),
				.model_matrix	= m_model_matrix,
				.color			= m_color
			};
		}

		auto set_color(const glm::vec3& new_color) -> void ;
		auto set_position(const vec3<length>& new_position) -> void;

	protected:
		friend render_component;
		auto set_up_mesh() -> void;

		GLuint m_vao = 0;
		GLuint m_vbo = 0;
		GLuint m_ebo = 0;

		std::vector<vertex> m_vertices;
		std::vector<unsigned int> m_indices;
		std::vector<model_texture> m_textures;

		glm::mat4 m_model_matrix = glm::mat4(1.0f);
		GLuint m_draw_mode = GL_TRIANGLES;
		std::string m_material_name = "Concrete";
		glm::vec3 m_color = { 1.0f, 1.0f, 1.0f };

		vec3<length> m_center_of_mass;
		bool m_com_calculated = false;
	};
}
