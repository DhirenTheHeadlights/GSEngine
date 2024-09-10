#pragma once

#include <iostream>

#include "GameObject.h"

#include "Engine.h"
#include "Camera.h"

#include "PlatformInput.h"

namespace Game {
	class Player : public GameObject {
	public:
		Player() : GameObject(Engine::idManager.generateID()) {}

		void initialize();

		void update(float deltaTime);
		void render(const glm::mat4& view, const glm::mat4& projection);

		Engine::Camera& getCamera() { return camera; }
	private:
		Engine::Camera camera = Engine::Camera(glm::vec3(0.f, 0.f, 0.f));

		std::unordered_map<int, glm::vec3> movementKeys;
	};
}