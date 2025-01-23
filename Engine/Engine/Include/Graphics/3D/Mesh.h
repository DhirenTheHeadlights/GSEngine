#pragma once

#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Physics/Vector/Vec3.h"

namespace gse {
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
		uint32_t id;
		std::string type;
		std::string path;
	};

	struct render_component;

	class mesh {
	public:
		mesh();
		mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices);
		mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<model_texture>& textures);
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
		std::vector<uint32_t> indices;
		std::vector<model_texture> textures;

		vec3<length> center_of_mass;
	};
}
