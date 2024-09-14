#pragma once

//#include <algorithm>
#include <concepts>
#include <vector>
#include <glm/glm.hpp>

#include "Engine/Graphics/BoundingBox.h"
#include "Engine/Core/Object/Object.h"

// To prevent conflicts with the Windows API
#undef min
#undef max

namespace Engine {
	class CollisionHandler {
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
		static bool checkCollision(const glm::vec3& point, const BoundingBox& box);
		static bool checkCollision(const std::vector<BoundingBox>& boxes1, const std::vector<BoundingBox>& boxes2);

		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		void addObject(Object& object) {
			objects.push_back(&object);
		}

		void addObject(DynamicObject& object) {
			objects.push_back(&object);
			dynamicObjects.push_back(&object);
		}

		void removeObject(Object& object) {
			std::erase(objects, &object);
		}

		void removeObject(DynamicObject& object) {
			std::erase(objects, &object);
			std::erase(dynamicObjects, &object);
		}

		void update() const;

	private:
		std::vector<Object*> objects;

		std::vector<DynamicObject*> dynamicObjects;
	};
}
