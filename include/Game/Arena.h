#pragma once

#include "Engine/Core/Engine.h"
#include "Engine/Core/Object/StaticObject.h"

namespace Game {
	class Arena final : public Engine::StaticObject {
	public:
		Arena() : StaticObject(Engine::idManager.generateID()) {}

		void initialize();
		void render(const glm::mat4& view, const glm::mat4& projection);
	private:
		float width = 1000.f, height = 1000.f, depth = 1000.f;
	};
}