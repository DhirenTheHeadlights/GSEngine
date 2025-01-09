#pragma once

#include "Engine.h"

namespace game {
	auto create_sphere_light(const gse::vec3<gse::length>& position, const gse::length& radius, int sectors) -> std::uint32_t;
}