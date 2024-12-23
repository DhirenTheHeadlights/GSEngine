#pragma once

#include "Engine.h"

namespace game {
	class sphere_light;

	struct sphere_light_hook final : gse::hook<sphere_light> {
		using hook::hook;
		void initialize() override;
		void update() override;
		void render() override;
	};

	class sphere_light final : public gse::sphere {
	public:
		sphere_light(const gse::vec3<gse::length>& position, const gse::length radius, const int sectors = 36, const int stacks = 18)
			: sphere(position, radius, sectors, stacks) {
			add_hook(std::make_unique<sphere_light_hook>(this));
		}
	};
}