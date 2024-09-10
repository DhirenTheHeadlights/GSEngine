#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Engine.h"
#include "GameObject.h"

namespace Game {
	class Arena : public GameObject {
	public:
		Arena(const glm::vec3 size) : GameObject(Engine::idManager.generateID()), width(size.x), height(size.y), depth(size.z) {}

		void initialize();
		void render(const glm::mat4& view, const glm::mat4& projection);
	private:
		float width, height, depth; 
	};
}