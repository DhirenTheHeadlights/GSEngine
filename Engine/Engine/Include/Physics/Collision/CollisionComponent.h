#pragma once

#include "Physics/Graphics/BoundingBox.h"

namespace Engine::Physics {
	struct CollisionComponent {
		std::vector<BoundingBox> boundingBoxes;
	};
}