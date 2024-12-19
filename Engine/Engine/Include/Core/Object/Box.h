#pragma once
#include "Engine.h"

namespace gse {
	class box final : public object {
	public:
		box(const vec3<length>& initial_position, const vec3<length>& size);
	};
}