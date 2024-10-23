#pragma once
#include <glad/glad.h>

#include "Graphics/MeshComponent.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent();
		RenderComponent(const MeshComponent& mesh, GLenum drawMode = GL_TRIANGLES);
		virtual ~RenderComponent();
		RenderComponent(const RenderComponent&);
		RenderComponent(RenderComponent&&) noexcept;

		void render() const;

		void setMesh(const MeshComponent& mesh);
	protected:
		void setUpBuffers();
		void deleteBuffers() const;

		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint EBO = 0;

		GLenum drawMode = GL_TRIANGLES;
		GLsizei vertexCount = 0;
	};
}