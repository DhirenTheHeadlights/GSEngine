#pragma once

#include "Engine/Include/Graphics/Shader.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent();
		virtual ~RenderComponent();
		RenderComponent(const RenderComponent&);
		RenderComponent(RenderComponent&&) noexcept;

		void render() const;
	protected:
		GLuint VAO = 0;
		GLuint VBO = 0;

		GLenum drawMode = GL_TRIANGLES;
		GLsizei vertexCount = 0;
	};
}