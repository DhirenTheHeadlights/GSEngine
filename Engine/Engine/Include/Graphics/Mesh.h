#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Graphics/RenderQueueEntry.h"

namespace Engine {
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoords;
	};

	class Mesh {
	public:
		Mesh();
		Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3 color = { 1.0f, 1.0f, 1.0f }, GLuint textureId = 0);
		~Mesh();

		// Disable copy to avoid OpenGL resource duplication
		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;

		// Allow move semantics
		Mesh(Mesh&& other) noexcept;
		Mesh& operator=(Mesh&& other) noexcept;

		GLuint getVAO() const { return VAO; }
		GLsizei getVertexCount() const { return static_cast<GLsizei>(indices.size()); }

		virtual RenderQueueEntry getQueueEntry() const {
			return {
				"Lighting",
				VAO,
				drawMode,
				getVertexCount(),
				modelMatrix,
				textureId,
				color
			};
		}

		void setColor(const glm::vec3& newColor) { color = newColor; }
	protected:
		void setUpMesh();

		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint EBO = 0;

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		glm::mat4 modelMatrix = glm::mat4(1.0f);
		GLuint textureId = 0;
		GLuint drawMode = GL_TRIANGLES;
		glm::vec3 color = { 1.0f, 1.0f, 1.0f };
	};
}
