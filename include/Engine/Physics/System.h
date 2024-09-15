#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Engine/Graphics/BoundingBox.h"

namespace Engine::Physics {
	extern std::vector<MotionComponent*> components;

	void addMotionComponent(MotionComponent& component);
	void removeMotionComponent(MotionComponent& component);

	void updateEntities(const float deltaTime);
	void resolveCollision(MotionComponent& component, const CollisionInformation& collisionInfo);

}
