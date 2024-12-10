#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"

namespace gse {
	class RenderComponent : public engine_component {
	public:
		RenderComponent(ID* id) : engine_component(id) {}
		RenderComponent(ID* id, const std::vector<std::shared_ptr<Mesh>>& meshes) : engine_component(id), meshes(meshes) {}

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
