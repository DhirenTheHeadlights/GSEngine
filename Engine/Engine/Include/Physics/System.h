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
		void addComponent(const std::shared_ptr<MotionComponent>& object);
		void removeComponent(const std::shared_ptr<MotionComponent>& object);

		void updateEntities();

		void updateEntity(MotionComponent* component);
		void resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo);
	private:
		std::vector<std::weak_ptr<MotionComponent>> objectMotionComponents;
		Vec3<Units::MetersPerSecondSquared> gravity = Vec3<Units::MetersPerSecondSquared>(0.f, -9.8f, 0.f);
	};
}
