#include "Engine/Graphics/RenderComponent.h"

Engine::RenderComponent::RenderComponent() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
}

Engine::RenderComponent::~RenderComponent() {
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

void Engine::RenderComponent::setMeshData(const float* vertices, const size_t vertexCount) const {
	// Bind the VAO and VBO to store vertex data
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	// Allocate and initialize the buffer with vertex data
	glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

	// Specify the layout of the vertex data (positions, texture coords, etc.)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), static_cast<void*>(nullptr)); // Positions
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // Tex coords
	glEnableVertexAttribArray(1);

	// Unbind VAO to prevent unintended modification
	glBindVertexArray(0);
}