#pragma once

#include <Engine.h>

namespace Game {
	class Player final : public Engine::Object {
	public:
		Player() : Object(Engine::idManager.generateID()) {}

		void initialize();

		void update();
		void render();

		void setPosition(const Engine::Vec3<Engine::Length>& position) { boundingBoxes[0].setPosition(position); }

		bool jetpack = false;
	private:
		void updateJetpack();
		void updateMovement();

		std::unordered_map<int, Engine::Vec3<Engine::Unitless>> wasd;

		Engine::Units::MetersPerSecond maxSpeed = 20.f;
		Engine::Units::MetersPerSecond shiftMaxSpeed = 40.f;
		Engine::Units::Newtons jetpackForce = 10000.f;
		Engine::Units::Newtons jetpackSideForce = 10000.f;
		Engine::Units::Newtons jumpForce = 1000.f;
		Engine::Units::Newtons moveForce = 100000.f;
	};
}