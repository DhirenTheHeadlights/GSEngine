#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "BoundingBoxMesh.h"
#include "Core/EngineComponent.h"
#include "Graphics/Mesh.h"

namespace Engine {
	class RenderComponent : public EngineComponent {
	public:
		RenderComponent(ID* id) : EngineComponent(id) {}
		RenderComponent(ID* id, const std::vector<std::shared_ptr<Mesh>>& meshes) : EngineComponent(id), meshes(meshes) {}

		void addMesh(const std::shared_ptr<Mesh>& mesh);
		void addBoundingBoxMesh(const std::shared_ptr<BoundingBoxMesh>& boundingBoxMesh);

		void updateBoundingBoxMeshes() const;
		void setRender(bool render, bool renderBoundingBoxes);

		virtual std::vector<RenderQueueEntry> getQueueEntries() const;
		std::vector<std::shared_ptr<Mesh>>& getMeshes() { return meshes; }
		std::vector<std::shared_ptr<BoundingBoxMesh>>& getBoundingBoxMeshes() { return boundingBoxMeshes; }
	protected:
		std::vector<std::shared_ptr<Mesh>> meshes;
		std::vector<std::shared_ptr<BoundingBoxMesh>> boundingBoxMeshes;

		bool render = false;
		bool renderBoundingBoxes = false;
	};
}
