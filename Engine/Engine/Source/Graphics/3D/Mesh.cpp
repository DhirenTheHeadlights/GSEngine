#include "Graphics/3D/Mesh.h"

#include "glm/ext/matrix_transform.hpp"
#include "Physics/Vector/Math.h"

gse::mesh::mesh() {
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3& color, const GLuint texture_id)
	: m_vertices(vertices), m_indices(indices), m_color(color) {
	set_up_mesh();
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<model_texture>& textures)
	: m_vertices(vertices), m_indices(indices), m_textures(textures) {
	set_up_mesh();
}

gse::mesh::~mesh() {
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteBuffers(1, &m_ebo);
}

gse::mesh::mesh(mesh&& other) noexcept
	: m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo),
	m_vertices(std::move(other.m_vertices)), m_indices(std::move(other.m_indices)) {
	other.m_vao = 0;
	other.m_vbo = 0;
	other.m_ebo = 0;
}

gse::mesh& gse::mesh::operator=(mesh&& other) noexcept {
	if (this != &other) {
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);

		m_vao = other.m_vao;
		m_vbo = other.m_vbo;
		m_ebo = other.m_ebo;
		m_vertices = std::move(other.m_vertices);
		m_indices = std::move(other.m_indices);

		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
	}
	return *this;
}

GLuint gse::mesh::get_vao() const { return m_vao; }

void gse::mesh::set_position(const vec3<length>& new_position) {
	m_model_matrix = translate(glm::mat4(1.0f), new_position.as<units::meters>());
}

void gse::mesh::set_up_mesh() {
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);

	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), m_vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

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
}
