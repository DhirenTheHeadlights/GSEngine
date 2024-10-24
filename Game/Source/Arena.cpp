#include "Arena.h"

#include "Graphics/Renderer.h"

void Game::Arena::initialize() {
	const Engine::Units::Meters wallThickness = 10.f;

	const auto collisionComponent = std::make_shared<Engine::Physics::CollisionComponent>();

    // Front and back walls
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, 0.f, depth / 2.f - wallThickness / 2.f), width, height, wallThickness);
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, 0.f, -depth / 2.f + wallThickness / 2.f), width, height, wallThickness);

    // Left and right walls
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(-width / 2.f + wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(width / 2.f - wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);

    // Top and bottom walls
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, height / 2.f - wallThickness / 2.f, 0.f), width, wallThickness, depth);
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, -height / 2.f + wallThickness / 2.f, 0.f), width, wallThickness, depth);

	// Set up mesh

	// Define vertices for the floor (flat quad)
	const std::vector<Engine::Vertex> floorVertices = {
		{ glm::vec3(-width.getValue() / 2.f, -height.getValue() / 2.f, depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },  // Bottom-left corner
		{ glm::vec3(width.getValue() / 2.f, -height.getValue() / 2.f, depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },   // Bottom-right corner
		{ glm::vec3(width.getValue() / 2.f, -height.getValue() / 2.f, -depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },  // Top-right corner
		{ glm::vec3(-width.getValue() / 2.f, -height.getValue() / 2.f, -depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }  // Top-left corner
	};

	// Define indices for two triangles that make up the floor quad
	const std::vector<unsigned int> floorIndices = {
		0, 1, 2,  // First triangle
		2, 3, 0   // Second triangle
	};

	const auto meshComponent = std::make_shared<Engine::MeshComponent>(floorVertices, floorIndices);
	const auto renderComponent = std::make_shared<Engine::RenderComponent>();
	renderComponent->setMesh(meshComponent);

	addComponent(collisionComponent);
	addComponent(renderComponent);
	addComponent(meshComponent);
}

void Game::Arena::render() const {
	getComponent<Engine::RenderComponent>()->render();
	for (auto& bb : getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes) {
		bb.render(false);
	}
}
