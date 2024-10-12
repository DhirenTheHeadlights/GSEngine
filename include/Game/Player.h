#pragma once

#include "Engine/Core/Engine.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Core/Object/DynamicObject.h"

namespace Game {
	class Player final : public Engine::DynamicObject {
	public:
		Player() : DynamicObject(Engine::idManager.generateID()) {}

		void initialize();

		void update();
		void render(const glm::mat4& view, const glm::mat4& projection);

		Engine::Camera& getCamera() { return camera; }

		void setPosition(const Engine::Vec3<Engine::Length>& position) { boundingBoxes[0].setPosition(position); }
	private:
		Engine::Camera camera = Engine::Camera(glm::vec3(0.f, 0.f, 0.f));

		std::unordered_map<int, glm::vec3> wasd;

		float speed = 10.f;
	};
}