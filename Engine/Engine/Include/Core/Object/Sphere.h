#pragma once
#include "Engine.h"

namespace gse {
	auto create_sphere(const gse::object* object, const vec3<length>& position, length radius, int sectors = 36, int stacks = 18) -> void;
	auto create_sphere(const vec3<length>& position, const length radius, const int sectors = 36, const int stacks = 18)
		-> object
		*;
}
