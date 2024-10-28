#include "Graphics/Mesh.h"

Engine::Mesh::Mesh() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
}

Engine::Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3 color, GLuint textureId)
	: vertices(vertices), indices(indices) {
	setUpMesh();
}

Engine::Mesh::~Mesh() {
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}

Engine::Mesh::Mesh(Mesh&& other) noexcept
	: VAO(other.VAO), VBO(other.VBO), EBO(other.EBO),
	vertices(std::move(other.vertices)), indices(std::move(other.indices)) {
	other.VAO = 0;
	other.VBO = 0;
	other.EBO = 0;
}

Engine::Mesh& Engine::Mesh::operator=(Mesh&& other) noexcept {
	if (this != &other) {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);

		VAO = other.VAO;
		VBO = other.VBO;
		EBO = other.EBO;
		vertices = std::move(other.vertices);
		indices = std::move(other.indices);

		other.VAO = 0;
		other.VBO = 0;
		other.EBO = 0;
	}
	return *this;
}

void Engine::Mesh::setUpMesh() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), static_cast<void*>(nullptr));
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
	glEnableVertexAttribArray(1);

	// Texture coordinates attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoords)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}
