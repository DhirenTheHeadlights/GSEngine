#include "Game/Arena.h"

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
}

void Game::Arena::render() {
    for (auto& bb : boundingBoxes) {
		bb.render(false);
	}
}
