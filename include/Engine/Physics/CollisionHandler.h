#pragma once

#include <glm/glm.hpp>

namespace Engine {
	class CollisionHandler {
	public:
		static bool checkCollision(glm::vec3 position1, float radius1, glm::vec3 position2, float radius2) {
			float distance = glm::length(position1 - position2);
			return distance < radius1 + radius2;
		}
	private:
		
	};
}