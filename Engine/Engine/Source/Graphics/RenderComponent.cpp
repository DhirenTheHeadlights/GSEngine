#include "Graphics/RenderComponent.h"

void gse::RenderComponent::addMesh(const std::shared_ptr<Mesh>& mesh) {
	mesh->setUpMesh();
	meshes.push_back(mesh);
}

void gse::RenderComponent::addBoundingBoxMesh(const std::shared_ptr<BoundingBoxMesh>& boundingBoxMesh) {
	boundingBoxMesh->setUpMesh();
	boundingBoxMeshes.push_back(boundingBoxMesh);
}

void gse::RenderComponent::updateBoundingBoxMeshes() const {
	for (const auto& boundingBoxMesh : boundingBoxMeshes) {
		boundingBoxMesh->update();
	}
}

std::vector<gse::RenderQueueEntry> gse::RenderComponent::getQueueEntries() const {
	if (!render) return {};

	std::vector<RenderQueueEntry> entries;
	entries.reserve(meshes.size() + boundingBoxMeshes.size());
	for (const auto& mesh : meshes) {
		entries.push_back(mesh->getQueueEntry());
	}

	if (boundingBoxMeshes.empty()) return entries;
	for (const auto& boundingBoxMesh : boundingBoxMeshes) {
		entries.push_back(boundingBoxMesh->getQueueEntry());
	}
	return entries;
}

void gse::RenderComponent::setRender(const bool render, const bool renderBoundingBoxes) {
	this->render = render;
	this->renderBoundingBoxes = renderBoundingBoxes;
}