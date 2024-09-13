#include "Arena.h"

using namespace Game;

void Arena::initialize() {
    constexpr float wallThickness = 10.f;

    // Front and back walls
    boundingBoxes.emplace_back(Engine::BoundingBox({ 0, 0, depth / 2 - wallThickness / 2 }, width, height, wallThickness));
    boundingBoxes.emplace_back(Engine::BoundingBox({ 0, 0, -depth / 2 + wallThickness / 2 }, width, height, wallThickness));

    // Left and right walls
    boundingBoxes.emplace_back(Engine::BoundingBox({ -width / 2 + wallThickness / 2, 0, 0 }, wallThickness, height, depth));
    boundingBoxes.emplace_back(Engine::BoundingBox({ width / 2 - wallThickness / 2, 0, 0 }, wallThickness, height, depth));

    // Top and bottom walls
    boundingBoxes.emplace_back(Engine::BoundingBox({ 0, height / 2 - wallThickness / 2, 0 }, width, wallThickness, depth));
    boundingBoxes.emplace_back(Engine::BoundingBox({ 0, -height / 2 + wallThickness / 2, 0 }, width, wallThickness, depth));
}

void Arena::render(const glm::mat4& view, const glm::mat4& projection) {
    for (auto& boundingBox : boundingBoxes) {
        drawBoundingBox(boundingBox, view * projection, false);
	}
}
