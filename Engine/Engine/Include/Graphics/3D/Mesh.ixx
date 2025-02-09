module;

#include <glad/glad.h>
#include "glm/ext/matrix_transform.hpp"

export module gse.graphics.mesh;

import std;
import glm;

import gse.physics.math;

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
		mat4 model_matrix;
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

gse::mesh::mesh() {
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices)
	: vertices(vertices), indices(indices) {
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<model_texture>& textures)
	: vertices(vertices), indices(indices), textures(textures) {
}

gse::mesh::~mesh() {
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ebo);
}

gse::mesh::mesh(mesh&& other) noexcept
	: vao(other.vao), vbo(other.vbo), ebo(other.ebo),
	vertices(std::move(other.vertices)), indices(std::move(other.indices)), textures(std::move(other.textures)) {
	other.vao = 0;
	other.vbo = 0;
	other.ebo = 0;
}

auto gse::mesh::operator=(mesh&& other) noexcept -> mesh& {
	if (this != &other) {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ebo);

		vao = other.vao;
		vbo = other.vbo;
		ebo = other.ebo;
		vertices = std::move(other.vertices);
		indices = std::move(other.indices);

		other.vao = 0;
		other.vbo = 0;
		other.ebo = 0;
	}
	return *this;
}

namespace {
	auto calculate_center_of_mass(const std::vector<uint32_t>& indices, const std::vector<gse::vertex>& vertices) -> gse::vec3<gse::length> {
		constexpr glm::dvec3 reference_point(0.f);

		float total_volume = 0.f;
		glm::vec3 moment(0.f);

		if (indices.size() % 3 != 0) {
			throw std::runtime_error("Indices count is not a multiple of 3. Ensure that each face is defined by exactly three indices.");
		}

		for (size_t i = 0; i < indices.size(); i += 3) {
			unsigned int idx0 = indices[i];
			unsigned int idx1 = indices[i + 1];
			unsigned int idx2 = indices[i + 2];

			if (idx0 >= vertices.size() || idx1 >= vertices.size() || idx2 >= vertices.size()) {
				throw std::out_of_range("Index out of range while accessing vertices.");
			}

			const glm::dvec3 v0(vertices[idx0].position);
			const glm::dvec3 v1(vertices[idx1].position);
			const glm::dvec3 v2(vertices[idx2].position);

			glm::dvec3 a = v0 - reference_point;
			glm::dvec3 b = v1 - reference_point;
			glm::dvec3 c = v2 - reference_point;

			double volume = glm::abs(glm::dot(a, glm::cross(b, c)) / 6.0);
			glm::dvec3 tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

			total_volume += static_cast<float>(volume);
			moment += volume * tetra_com;
		}

		if (total_volume == 0.0) {
			throw std::runtime_error("Total volume is zero. Check if the mesh is closed and correctly oriented.");
		}

		auto result = moment / total_volume;
		return gse::vec3<gse::length>(result.x, result.y, result.z);
	}
}

auto gse::mesh::initialize() -> void {
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), static_cast<void*>(nullptr));
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void*>(offsetof(vertex, normal)));
	glEnableVertexAttribArray(1);

	// Texture coordinates attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void*>(offsetof(vertex, tex_coords)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	center_of_mass = calculate_center_of_mass(indices, vertices);
}