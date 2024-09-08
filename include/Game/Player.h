#pragma once

#include "GameObject.h"

#include "Engine/Graphics/Camera.h"

#include "PlatformInput.h"

namespace Game {
	class Player : public GameObject {
	public:
		Player() = default;

		void initialize();

		void update(float deltaTime);
		void render(const glm::mat4& view, const glm::mat4& projection) const;


		void setIsColliding(bool isColliding) override { this->colliding = isColliding; }

		bool isColliding() const override { return colliding; }
		std::vector<Engine::BoundingBox>& getBoundingBoxes() override { return boundingBoxes; }
		Engine::Camera& getCamera() { return camera; }
	private:
		std::vector<Engine::BoundingBox> boundingBoxes = { Engine::BoundingBox({ -10.f, -10.f, -10.f }, { 10.f, 10.f, 10.f }) };

		Engine::Camera camera = Engine::Camera(glm::vec3(0.f, 0.f, 0.f));

		bool colliding = false;
	};
}