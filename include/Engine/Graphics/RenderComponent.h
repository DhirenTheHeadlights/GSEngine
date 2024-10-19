#pragma once

#include "Engine/Graphics/Shader.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent();
		~RenderComponent();

		void setMeshData(const float* vertices, size_t vertexCount) const;
		void setTexture(const GLuint textureID) { this->textureID = textureID; }
		void setTransform(const glm::mat4& transform) { this->transform = transform; }

		GLuint getVAO() const { return VAO; }
		GLuint getTextureID() const { return textureID; }
		glm::mat4 getTransform() const { return transform; }
	protected:
		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint textureID = 0;
		glm::mat4 transform = glm::mat4(1.0f);
	};
}