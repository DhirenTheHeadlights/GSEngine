#pragma once

//#include <algorithm>
#include <vector>

#include "Engine/Include/Core/Object/DynamicObject.h"
#include "Engine/Include/Core/Object/Object.h"
#include "Engine/Include/Graphics/BoundingBox.h"

namespace Engine {
	class BroadPhaseCollisionHandler {
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
		static bool checkCollision(const BoundingBox& dynamicBox, const BoundingBox& staticBox, const Physics::MotionComponent* component);
		static bool checkCollision(const Vec3<Length>& point, const BoundingBox& box);
		static bool checkCollision(const std::shared_ptr<DynamicObject>& object1, const std::shared_ptr<Object>& object2);

		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		void addObject(const std::weak_ptr<Object>& object) {
			objects.push_back(object);
		}

		void addObject(const std::weak_ptr<DynamicObject>& object) {
			dynamicObjects.push_back(object);
		}

		void removeObject(const std::weak_ptr<Object>& object) {
			std::erase_if(objects, [&](const std::weak_ptr<Object>& obj) {
				return !obj.owner_before(object) && !object.owner_before(obj);
				});
		}

		void removeObject(const std::weak_ptr<DynamicObject>& object) {
			std::erase_if(dynamicObjects, [&](const std::weak_ptr<DynamicObject>& obj) {
				return !obj.owner_before(object) && !object.owner_before(obj);
				});
		}

		void update() const;
	private:
		std::vector<std::weak_ptr<Object>> objects;

		std::vector<std::weak_ptr<DynamicObject>> dynamicObjects;
	};
}
