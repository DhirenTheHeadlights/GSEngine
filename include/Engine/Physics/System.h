#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Engine/Graphics/BoundingBox.h"

namespace Engine::Physics {
	extern std::vector<MotionComponent*> components;

	void applyForce(MotionComponent* component, const Vec3<Force>& force);

	void addMotionComponent(MotionComponent& component);
	void removeMotionComponent(MotionComponent& component);

	void updateEntities();
	void resolveCollision(const BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo);

}
