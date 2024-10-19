#include "Engine/Graphics/RenderComponent.h"

#include <iostream>

Engine::RenderComponent::RenderComponent() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
}

Engine::RenderComponent::~RenderComponent() {
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

Engine::RenderComponent::RenderComponent(const RenderComponent& other)
	: drawMode(other.drawMode), vertexCount(other.vertexCount) {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
}

Engine::RenderComponent::RenderComponent(RenderComponent&& other) noexcept
	: VAO(other.VAO), VBO(other.VBO), drawMode(other.drawMode), vertexCount(other.vertexCount) {
	other.VAO = 0;
	other.VBO = 0;
}

void Engine::RenderComponent::render() const {
	glBindVertexArray(VAO);
	glDrawArrays(drawMode, 0, vertexCount);
	glBindVertexArray(0);
}