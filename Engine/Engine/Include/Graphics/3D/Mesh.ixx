module;

#include <cstddef>
#include <glad/glad.h>

export module gse.graphics.mesh;

import std;

import gse.physics.math;
import gse.platform.perma_assert;
import gse.graphics.material;

export namespace gse {
	struct vertex {
		vec::raw3f position;
		vec::raw3f normal;
		vec::raw2f tex_coords;
	};

	struct render_queue_entry {
		std::string material_key;
		GLuint vao;
		GLenum draw_mode;
		GLsizei vertex_count;
		mat4 model_matrix;
		unitless::vec3 color;
		std::span<const std::uint32_t> texture_ids;
		gse::mtl_material* material;
	};

	struct render_component;

	struct mesh {
		mesh();
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices);
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, gse::mtl_material* material);
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const std::vector<std::uint32_t>& texture_ids);
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
		std::vector<std::uint32_t> texture_ids;
		gse::mtl_material* material = nullptr;

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

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices, gse::mtl_material* material)
	: vertices(vertices), indices(indices), material(material) {
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<uint32_t>& texture_ids)
	: vertices(vertices), indices(indices), texture_ids(texture_ids) {
}

gse::mesh::~mesh() {
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ebo);
}

gse::mesh::mesh(mesh&& other) noexcept
	: vao(other.vao), vbo(other.vbo), ebo(other.ebo),
	vertices(std::move(other.vertices)), indices(std::move(other.indices)), texture_ids(std::move(other.texture_ids)), material(other.material) {
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

auto calculate_center_of_mass(const std::vector<std::uint32_t>& indices, const std::vector<gse::vertex>& vertices) -> gse::vec3<gse::length> {
	constexpr gse::unitless::vec3d reference_point(0.f);

	double total_volume = 0.f;
	gse::unitless::vec3 moment(0.f);

	perma_assert(indices.size() % 3 == 0, "Indices count is not a multiple of 3. Ensure that each face is defined by exactly three indices.");

	for (size_t i = 0; i < indices.size(); i += 3) {
		const unsigned int idx0 = indices[i];
		const unsigned int idx1 = indices[i + 1];
		const unsigned int idx2 = indices[i + 2];

		perma_assert(idx0 <= vertices.size() || idx1 <= vertices.size() || idx2 <= vertices.size(), "Index out of range while accessing vertices.");

		const gse::unitless::vec3d v0(vertices[idx0].position);
		const gse::unitless::vec3d v1(vertices[idx1].position);
		const gse::unitless::vec3d v2(vertices[idx2].position);

		gse::unitless::vec3d a = v0 - reference_point;
		gse::unitless::vec3d b = v1 - reference_point;
		gse::unitless::vec3d c = v2 - reference_point;

		auto volume = std::abs(gse::dot(a, gse::cross(b, c)) / 6.0);
		gse::unitless::vec3d tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

		total_volume += volume;
		moment += tetra_com * volume;
	}

	perma_assert(total_volume != 0.0, "Total volume is zero. Check if the mesh is closed and correctly oriented.");

	return moment / total_volume;
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