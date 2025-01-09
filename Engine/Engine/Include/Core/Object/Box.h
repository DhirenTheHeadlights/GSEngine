#pragma once
#include "Engine.h"

namespace gse {
	auto create_box(std::uint32_t object_uuid, const vec3<length>& initial_position, const vec3<length>& size) -> void;
	auto create_box(const vec3<length>& initial_position, const vec3<length>& size) -> std::uint32_t;
}