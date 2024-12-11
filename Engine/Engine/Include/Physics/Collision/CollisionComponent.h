#pragma once

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	struct collision_component final : engine_component {
		collision_component(id* id) : engine_component(id) {}
		std::vector<bounding_box> bounding_boxes;
	};
}
