#pragma once
#include <vector>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>


#include "Graphics/RenderQueueEntry.h"

namespace Engine {
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoords;
	};

	class MeshComponent {
	public:
		MeshComponent(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const glm::vec3 color = { 1.0f, 1.0f, 1.0f }, GLuint textureId = 0);
		~MeshComponent();

		// Disable copy to avoid OpenGL resource duplication
		MeshComponent(const MeshComponent&) = delete;
		MeshComponent& operator=(const MeshComponent&) = delete;

		// Allow move semantics
		MeshComponent(MeshComponent&& other) noexcept;
		MeshComponent& operator=(MeshComponent&& other) noexcept;

		GLuint getVAO() const { return VAO; }
		GLsizei getVertexCount() const { return static_cast<GLsizei>(indices.size()); }

		RenderQueueEntry getQueueEntry() const {
			return {
				"SolidColor",
				VAO,
				GL_TRIANGLES,
				getVertexCount(),
				modelMatrix,
				textureId,
				color
			};
		}

		void setColor(const glm::vec3& newColor) { color = newColor; }
	private:
		void setUpMesh();

		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint EBO = 0;

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		glm::mat4 modelMatrix = glm::mat4(1.0f);
		GLuint textureId = 0;
		glm::vec3 color = { 1.0f, 1.0f, 1.0f };
	};
}
