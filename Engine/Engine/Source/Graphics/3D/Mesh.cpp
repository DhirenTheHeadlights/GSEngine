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

auto gse::mesh::operator=(mesh&& other) noexcept -> gse::mesh& {
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

auto gse::mesh::get_vao() const -> GLuint {
	return m_vao;
}

auto gse::mesh::get_vertex_count() const -> GLsizei {
	return static_cast<GLsizei>(m_indices.size());
}

auto gse::mesh::calculate_center_of_mass() -> vec3<length> {
	if (m_com_calculated) {
		return m_center_of_mass;
	}

	constexpr glm::dvec3 reference_point(0.f);

	float total_volume = 0.f;
	glm::vec3 moment(0.f);

	if (m_indices.size() % 3 != 0) {
		throw std::runtime_error("Indices count is not a multiple of 3. Ensure that each face is defined by exactly three indices.");
	}

	for (size_t i = 0; i < m_indices.size(); i += 3) {
		unsigned int idx0 = m_indices[i];
		unsigned int idx1 = m_indices[i + 1];
		unsigned int idx2 = m_indices[i + 2];

		if (idx0 >= m_vertices.size() || idx1 >= m_vertices.size() || idx2 >= m_vertices.size()) {
			throw std::out_of_range("Index out of range while accessing vertices.");
		}

		const glm::dvec3 v0(m_vertices[idx0].position);
		const glm::dvec3 v1(m_vertices[idx1].position);
		const glm::dvec3 v2(m_vertices[idx2].position);

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

	m_com_calculated = true;

	m_center_of_mass = vec3<length>(moment / total_volume);
	return m_center_of_mass;
}

auto gse::mesh::set_color(const glm::vec3& new_color) -> void {
	m_color = new_color;
}

auto gse::mesh::set_position(const vec3<length>& new_position) -> void {
	m_model_matrix = translate(glm::mat4(1.0f), new_position.as<units::meters>());
}

auto gse::mesh::set_up_mesh() -> void {
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

	calculate_center_of_mass();
}
