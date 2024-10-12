#pragma once

//#include <algorithm>
#include <concepts>
#include <vector>
#include <glm/glm.hpp>

#include "imgui.h"

#include "Engine/Core/Object/DynamicObject.h"
#include "Engine/Core/Object/Object.h"
#include "Engine/Graphics/BoundingBox.h"

// To prevent conflicts with the Windows API
#undef min
#undef max

namespace Engine {
	class BroadPhaseCollisionHandler {
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
		static bool checkCollision(const BoundingBox& dynamicBox, const BoundingBox& staticBox, const Physics::MotionComponent* component);
		static bool checkCollision(const Vec3<Length>& point, const BoundingBox& box);
		static bool checkCollision(DynamicObject& object1, Object& object2);

		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		void addObject(Object& object) {
			objects.push_back(&object);
		}

		void addObject(DynamicObject& object) {
			dynamicObjects.push_back(&object);
		}

		void removeObject(Object& object) {
			std::erase(objects, &object);
		}

		void removeObject(DynamicObject& object) {
			std::erase(dynamicObjects, &object);
		}

		void update() const;
	private:
		std::vector<Object*> objects;

		std::vector<DynamicObject*> dynamicObjects;
	};
}
