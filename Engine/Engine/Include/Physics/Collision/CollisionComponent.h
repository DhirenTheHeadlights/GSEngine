#pragma once

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	struct collision_component final : component {
		collision_component(id* id) : component(id) {}
		bounding_box bounding_box;

		bool resolve_collisions = true;
	};
}
