#pragma once

#include <Engine.h>

namespace Game {
	class Player final : public Engine::Object {
	public:
		Player() : Object("Player") {}

		void initialize() override;
		void update() override;
		void render() override;

		bool jetpack = false;
	private:
		void updateJetpack();
		void updateMovement();

		std::unordered_map<int, Engine::Vec3<Engine::Unitless>> wasd;

		Engine::Velocity maxSpeed = Engine::milesPerHour(20.f);
		Engine::Velocity shiftMaxSpeed = Engine::milesPerHour(40.f);
		Engine::Force jetpackForce = Engine::newtons(1000.f);
		Engine::Force jetpackSideForce = Engine::newtons(500.f);
		Engine::Force jumpForce = Engine::newtons(1000.f);
		Engine::Force moveForce = Engine::newtons(100000.f);

		int boostFuel = 1000;
	};
}