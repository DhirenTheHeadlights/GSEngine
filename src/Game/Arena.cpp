#include "Arena.h"

using namespace Game;

void Arena::initialize() {
    // Front and back walls
    boundingBoxes.push_back(Engine::BoundingBox({ width / 2, height / 2, 0.1f }, { -width / 2, -height / 2, -depth / 2 }));
    boundingBoxes.push_back(Engine::BoundingBox({ width / 2, height / 2, 0.1f }, { -width / 2, -height / 2, depth / 2 }));

    // Left and right walls
    boundingBoxes.push_back(Engine::BoundingBox({ 0.1f, height, depth }, { -width / 2, -height / 2, -depth / 2 }));
    boundingBoxes.push_back(Engine::BoundingBox({ 0.1f, height, depth }, { width / 2, -height / 2, -depth / 2 }));

    // Top and bottom walls
    boundingBoxes.push_back(Engine::BoundingBox({ width, 0.1f, depth }, { -width / 2, height / 2, -depth / 2 }));
    boundingBoxes.push_back(Engine::BoundingBox({ width, 0.1f, depth }, { -width / 2, -height / 2, -depth / 2 }));
}


void Arena::render(const glm::mat4& view, const glm::mat4& projection) {
    for (auto& boundingBox : boundingBoxes) {
        Engine::drawBoundingBox(boundingBox, view * projection, false);
	}
}
