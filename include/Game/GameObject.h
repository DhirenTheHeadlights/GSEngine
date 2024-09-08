#pragma once

#include <vector>

#include "Engine/Physics/CollisionHandler.h"

namespace Game {
	class GameObject {
	public:

		//////////////////////////////////////////////////////////////////////////////////
		// All game objects require a colliding info and a vector of bounding boxes     //
		// These get passed into collision handler and are used to check for collisions //
		//////////////////////////////////////////////////////////////////////////////////

		virtual bool isColliding() const = 0;
		virtual void setIsColliding(bool isColliding) = 0;
		virtual std::vector<Engine::BoundingBox>& getBoundingBoxes() = 0;
	};
}