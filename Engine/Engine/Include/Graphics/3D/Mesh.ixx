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
		std::uint32_t id;
		std::string type;
		std::string path;
	};

	struct render_component;

	struct mesh {
		mesh();
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices);
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const std::vector<model_texture>& textures);
		virtual ~mesh();

		mesh(const mesh&) = delete;
		auto operator=(const mesh&) -> mesh& = delete;

		mesh(mesh&& other) noexcept;
		auto operator=(mesh&& other) noexcept -> mesh&;

		auto initialize() -> void;

		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;

		std::vector<vertex> vertices;
		std::vector<std::uint32_t> indices;
		std::vector<model_texture> textures;

		vec3<length> center_of_mass;
	};
}
