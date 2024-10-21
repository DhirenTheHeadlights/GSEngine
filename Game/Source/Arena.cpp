#include "Arena.h"

void Game::Arena::initialize() {
	const Engine::Units::Meters wallThickness = 10.f;

    // Front and back walls
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, 0.f, depth / 2.f - wallThickness / 2.f), width, height, wallThickness);
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, 0.f, -depth / 2.f + wallThickness / 2.f), width, height, wallThickness);

    // Left and right walls
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(-width / 2.f + wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(width / 2.f - wallThickness / 2.f, 0.f, 0.f), wallThickness, height, depth);

    // Top and bottom walls
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, height / 2.f - wallThickness / 2.f, 0.f), width, wallThickness, depth);
    boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(0.f, -height / 2.f + wallThickness / 2.f, 0.f), width, wallThickness, depth);

	// Set up mesh

	// Define vertices for the floor (flat quad)
	const std::vector<Engine::Vertex> floorVertices = {
		// Bottom-left corner
		{glm::vec3(-width.getValue() / 2.f, -height.getValue() / 2.f, depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
		// Bottom-right corner
		{glm::vec3(width.getValue() / 2.f, -height.getValue() / 2.f, depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
		// Top-right corner
		{glm::vec3(width.getValue() / 2.f, -height.getValue() / 2.f, -depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
		// Top-left corner
		{glm::vec3(-width.getValue() / 2.f, -height.getValue() / 2.f, -depth.getValue() / 2.f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}
	};

	// Define indices for two triangles that make up the floor quad
	const std::vector<unsigned int> floorIndices = {
		0, 1, 2,  // First triangle
		2, 3, 0   // Second triangle
	};

	// Set up the MeshComponent
	meshComponent = Engine::MeshComponent(floorVertices, floorIndices);

	// Initialize the RenderComponent with the mesh
	renderComponent.setMesh(meshComponent);

}

void Game::Arena::render() {
    for (auto& bb : boundingBoxes) {
		bb.render(false);
	}
}
