#pragma once
#include "Engine.h"

namespace gse {
	auto create_sphere(std::uint32_t object_uuid, const vec3<length>& position, length radius, int sectors = 36, int stacks = 18) -> void;
	auto create_sphere(const vec3<length>& position, length radius, int sectors = 36, int stacks = 18) -> std::uint32_t;
}
