#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Engine.h"
#include "GameObject.h"

namespace Game {
	class Arena : public GameObject {
	public:
		Arena() : GameObject(Engine::idManager.generateID()) {}

		void initialize();
		void render(const glm::mat4& view, const glm::mat4& projection);
	private:
		float width = 100.f, height = 100.f, depth = 100.f;
	};
}