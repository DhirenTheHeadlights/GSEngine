#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Graphics/MeshComponent.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent() = default;
		RenderComponent(const std::vector<std::shared_ptr<MeshComponent>>& meshes, GLenum drawMode = GL_TRIANGLES)
			: meshes(meshes), drawMode(drawMode) {}

		void addMesh(const std::shared_ptr<MeshComponent>& mesh) {
			meshes.push_back(mesh);
		}

		std::vector<RenderQueueEntry> getQueueEntries() const {
			std::vector<RenderQueueEntry> entries;
			entries.reserve(meshes.size());
			for (const auto& mesh : meshes) {
				entries.push_back(mesh->getQueueEntry());
			}
			return entries;
		}

		std::vector<std::shared_ptr<MeshComponent>>& getMeshes() { return meshes; }
	protected:
		std::vector<std::shared_ptr<MeshComponent>> meshes;
		GLenum drawMode = GL_TRIANGLES;
	};
}
