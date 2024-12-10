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

		std::unordered_map<int, gse::vec3<gse::unitless>> wasd;

		gse::velocity maxSpeed = gse::miles_per_hour(20.f);
		gse::velocity shiftMaxSpeed = gse::miles_per_hour(40.f);
		gse::force jetpackForce = gse::newtons(1000.f);
		gse::force jetpackSideForce = gse::newtons(500.f);
		gse::force jumpForce = gse::newtons(1000.f);
		gse::force moveForce = gse::newtons(100000.f);

		int boostFuel = 1000;
	};
}