#pragma once

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBox.h"

namespace Engine::Physics {
	struct CollisionComponent : EngineComponent {
		CollisionComponent(ID* id) : EngineComponent(id) {}
		std::vector<BoundingBox> boundingBoxes;
	};
}
