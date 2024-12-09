#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/Object.h"
#include "Graphics/3D/BoundingBox.h"

namespace Engine::Physics {
	class Group {
	public:
		void addMotionComponent(const std::shared_ptr<MotionComponent>& object);
		void removeMotionComponent(const std::shared_ptr<MotionComponent>& object);

		void update();
	private:
		std::vector<std::weak_ptr<MotionComponent>> motionComponents;
	};

	void applyForce(MotionComponent* component, const Vec3<Force>& force);
	void applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration);
	void updateEntity(MotionComponent* component);
	void resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo);
}
