#pragma once

#include <Engine.h>

namespace Game {
	class Player final : public gse::object {
	public:
		Player() : object("Player") {}

		void initialize() override;
		void update() override;
		void render() override;

		bool jetpack = false;
	private:
		void updateJetpack();
		void updateMovement();

		std::unordered_map<int, gse::Vec3<gse::Unitless>> wasd;

		gse::Velocity maxSpeed = gse::milesPerHour(20.f);
		gse::Velocity shiftMaxSpeed = gse::milesPerHour(40.f);
		gse::Force jetpackForce = gse::newtons(1000.f);
		gse::Force jetpackSideForce = gse::newtons(500.f);
		gse::Force jumpForce = gse::newtons(1000.f);
		gse::Force moveForce = gse::newtons(100000.f);

		int boostFuel = 1000;
	};
}