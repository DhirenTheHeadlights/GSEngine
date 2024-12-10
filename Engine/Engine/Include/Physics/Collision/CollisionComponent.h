#pragma once

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::Physics {
	struct CollisionComponent : engine_component {
		CollisionComponent(ID* id) : engine_component(id) {}
		std::vector<BoundingBox> boundingBoxes;
	};
}
