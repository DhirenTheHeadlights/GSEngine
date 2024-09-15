#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Engine/Graphics/BoundingBox.h"

namespace Engine::Physics {

	void updateEntities(const float deltaTime);

	void addMotionComponent(MotionComponent& component);
	void removeMotionComponent(MotionComponent& component);

	void resolveCollision(MotionComponent& component, const CollisionInformation& collisionInfo);

	extern std::vector<MotionComponent*> components;

}
