#pragma once

#include "Graphics/BoundingBox.h"

namespace Engine::Physics {
	struct CollisionComponent {
		std::vector<BoundingBox> boundingBoxes;
	};
}
