#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/Object.h"
#include "Graphics/BoundingBox.h"

namespace Engine::Physics {
	extern std::vector<std::weak_ptr<MotionComponent>> objectMotionComponents;

	void applyForce(MotionComponent* component, const Vec3<Force>& force);
	void applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration);

	inline void addComponent(const std::shared_ptr<MotionComponent>& object) {
		objectMotionComponents.push_back(object);
	}
	inline void removeComponent(const std::shared_ptr<MotionComponent>& object) {
		std::erase_if(objectMotionComponents, [&](const std::weak_ptr<MotionComponent>& obj) {
			return !obj.owner_before(object) && !object.owner_before(obj);
			});
	}

	void updateEntities();
	void updateEntity(MotionComponent* component);
	void resolveCollision(BoundingBox& dynamicBoundingBox, const std::weak_ptr<MotionComponent>& dynamicMotionComponent, const CollisionInformation& collisionInfo);
}
