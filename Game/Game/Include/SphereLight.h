#pragma once

#include "Engine.h"

namespace Game {
	class SphereLight;

	struct SphereLightHook : Engine::Hook<SphereLight> {
		using Hook::Hook;
		void initialize() override;
		void update() override;
		void render() override;
	};

	class SphereLight : public Engine::Sphere {
	public:
		SphereLight(const Engine::Vec3<Engine::Length>& position, const Engine::Length radius, const int sectors = 36, const int stacks = 18)
			: Sphere(position, radius, sectors, stacks) {
			addHook(std::make_unique<SphereLightHook>(this));
		}
	};
}