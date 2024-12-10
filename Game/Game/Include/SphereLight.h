#pragma once

#include "Engine.h"

namespace Game {
	class SphereLight;

	struct SphereLightHook : gse::hook<SphereLight> {
		using hook::hook;
		void initialize() override;
		void update() override;
		void render() override;
	};

	class SphereLight : public gse::sphere {
	public:
		SphereLight(const gse::vec3<gse::length>& position, const gse::length radius, const int sectors = 36, const int stacks = 18)
			: sphere(position, radius, sectors, stacks) {
			add_hook(std::make_unique<SphereLightHook>(this));
		}
	};
}