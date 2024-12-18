#pragma once

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	struct collision_component final : component {
		collision_component(id* id) : component(id) {}
		std::vector<bounding_box> bounding_boxes;
	};
}
