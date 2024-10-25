#pragma once
#include <memory>
#include <glad/glad.h>

#include "Graphics/MeshComponent.h"
#include "Graphics/RenderQueueEntry.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent();
		RenderComponent(const std::shared_ptr<MeshComponent>& mesh, GLenum drawMode = GL_TRIANGLES);
		virtual ~RenderComponent();
		RenderComponent(const RenderComponent&);
		RenderComponent(RenderComponent&&) noexcept;

		void render() const;

		void setMesh(const std::shared_ptr<MeshComponent>& mesh);

		RenderQueueEntry getQueueEntry() {
			return {
				"Color",
				VAO,
				drawMode,
				vertexCount,
				glm::mat4(1.0f),
				0
			};
		}
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
