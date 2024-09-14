#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"

namespace Engine::Physics {

	void updateEntities(const float deltaTime);

	void addMotionComponent(MotionComponent& component);
	void removeMotionComponent(MotionComponent& component);

	extern std::vector<MotionComponent*> components;

}
