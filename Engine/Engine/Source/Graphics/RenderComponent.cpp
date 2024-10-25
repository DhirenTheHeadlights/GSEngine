#include "Graphics/RenderComponent.h"

#include <iostream>

Engine::RenderComponent::RenderComponent() {
	setUpBuffers();
}

Engine::RenderComponent::RenderComponent(const std::shared_ptr<MeshComponent>& mesh, const GLenum drawMode) : drawMode(drawMode) {
	setUpBuffers();
	setMesh(mesh);
}

Engine::RenderComponent::~RenderComponent() {
	deleteBuffers();
}

Engine::RenderComponent::RenderComponent(const RenderComponent& other)
	: drawMode(other.drawMode), vertexCount(other.vertexCount) {
	setUpBuffers();
}

Engine::RenderComponent::RenderComponent(RenderComponent&& other) noexcept
	: VAO(other.VAO), VBO(other.VBO), drawMode(other.drawMode), vertexCount(other.vertexCount) {
	other.VAO = 0;
	other.VBO = 0;
}

void Engine::RenderComponent::setUpBuffers() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
}

void Engine::RenderComponent::deleteBuffers() const {
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}

void Engine::RenderComponent::setMesh(const std::shared_ptr<MeshComponent>& mesh) {
	glBindVertexArray(VAO);

	// Set up vertex data
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, mesh->getVertices().size() * sizeof(Vertex), mesh->getVertices().data(), GL_STATIC_DRAW);

	// Set up index data
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->getIndices().size() * sizeof(unsigned int), mesh->getIndices().data(), GL_STATIC_DRAW);

	// Vertex attributes
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), static_cast<void*>(nullptr)); // Position
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);

	vertexCount = static_cast<GLsizei>(mesh->getIndices().size());
}

void Engine::RenderComponent::render() const {

}