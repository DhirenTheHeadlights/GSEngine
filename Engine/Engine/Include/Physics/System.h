#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/DynamicObject.h"
#include "Graphics/BoundingBox.h"

namespace Engine::Physics {
	extern std::vector<std::weak_ptr<DynamicObject>> objects;

	void applyForce(MotionComponent* component, const Vec3<Force>& force);
	void applyImpulse(MotionComponent* component, const Vec3<Force>& force, const Time& duration);

	inline void addObject(const std::weak_ptr<DynamicObject>& object) {
		objects.push_back(object);
	}
	inline void removeObject(const std::weak_ptr<DynamicObject>& object) {
		std::erase_if(objects, [&](const std::weak_ptr<DynamicObject>& obj) {
			return !obj.owner_before(object) && !object.owner_before(obj);
			});
	}

	void updateEntities();
	void updateEntity(MotionComponent* component);
	void resolveCollision(BoundingBox& dynamicBoundingBox, MotionComponent& dynamicMotionComponent, const CollisionInformation& collisionInfo);
}
