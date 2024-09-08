#pragma once

#include "GameObject.h"

namespace Game {
	class Player : public GameObject {
	public:
		Player() = default;

		bool isColliding() const override { return isColliding; }
		void setIsColliding(bool isColliding) override { this->isColliding = isColliding; }
		std::vector<Engine::BoundingBox>& getBoundingBoxes() const override { return { boundingBox }; }
	private:
		Engine::BoundingBox boundingBox;

		bool isColliding = false;
	};
}