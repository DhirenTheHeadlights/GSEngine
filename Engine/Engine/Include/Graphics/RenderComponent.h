#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "BoundingBoxMesh.h"
#include "Graphics/Mesh.h"

namespace Engine {
	class RenderComponent {
	public:
		RenderComponent() = default;
		RenderComponent(const std::vector<std::shared_ptr<Mesh>>& meshes) : meshes(meshes) {}

		void addMesh(const std::shared_ptr<Mesh>& mesh) {
			meshes.push_back(mesh);
		}

		void addBoundingBoxMesh(const std::shared_ptr<BoundingBoxMesh>& boundingBoxMesh) {
			boundingBoxMeshes.push_back(boundingBoxMesh);
		}

		void updateBoundingBoxMeshes(bool moving) const;
		virtual std::vector<RenderQueueEntry> getQueueEntries() const;
		void setRender(bool render, bool renderBoundingBoxes);

		std::vector<std::shared_ptr<Mesh>>& getMeshes() { return meshes; }
		std::vector<std::shared_ptr<BoundingBoxMesh>>& getBoundingBoxMeshes() { return boundingBoxMeshes; }
	protected:
		std::vector<std::shared_ptr<Mesh>> meshes;
		std::vector<std::shared_ptr<BoundingBoxMesh>> boundingBoxMeshes;

		bool render = false;
		bool renderBoundingBoxes = false;
	};
}
