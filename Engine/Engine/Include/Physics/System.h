#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/Object.h"
#include "Graphics/BoundingBox.h"

namespace Engine::Physics {
	void applyForce(MotionComponent* component, const Vec3<Force>& force);
	void applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration);

	class System {
	public:
		void addMotionComponent(const std::shared_ptr<MotionComponent>& object);
		void removeMotionComponent(const std::shared_ptr<MotionComponent>& object);

		void update();

		static void updateEntity(MotionComponent* component);
		static void resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo);
	private:
		std::vector<std::weak_ptr<MotionComponent>> objectMotionComponents;
	};
}
