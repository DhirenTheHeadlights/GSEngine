#pragma once

#include "Engine/Include/Core/Engine.h"

namespace Game {
	class Player final : public Engine::DynamicObject {
	public:
		Player() : DynamicObject(Engine::idManager.generateID()) {}

		void initialize();

		void update();
		void render();

		Engine::Camera& getCamera() { return camera; }

		void setPosition(const Engine::Vec3<Engine::Length>& position) { boundingBoxes[0].setPosition(position); }

		bool jetpack = false;
	private:
		Engine::Camera camera;

		std::unordered_map<int, Engine::Vec3<Engine::Unitless>> wasd;

		Engine::Units::MetersPerSecond maxSpeed = 20.f;
		Engine::Units::MetersPerSecond shiftMaxSpeed = 40.f;
		Engine::Units::Newtons jetpackForce = 1000.f;
	};
}