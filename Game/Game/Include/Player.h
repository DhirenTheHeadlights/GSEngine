#pragma once

#include <Engine.h>

namespace game {
	class player final : public gse::object {
	public:
		player() : object("Player") {}

		void initialize() override;
		void update() override;
		void render() override;

		bool jetpack = false;
	private:
		void update_jetpack();
		void update_movement();

		std::unordered_map<int, gse::vec3<>> m_wasd;

		gse::velocity m_max_speed = gse::miles_per_hour(20.f);
		gse::velocity m_shift_max_speed = gse::miles_per_hour(40.f);
		gse::force m_jetpack_force = gse::newtons(1000.f);
		gse::force m_jetpack_side_force = gse::newtons(500.f);
		gse::force m_jump_force = gse::newtons(1000.f);
		gse::force m_move_force = gse::newtons(100000.f);

		int m_boost_fuel = 1000;
	};
}