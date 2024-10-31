#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace Engine {
	class BroadPhaseCollisionHandler {
		struct Object {
			Object(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent) :
				collisionComponent(collisionComponent) {}
			std::weak_ptr<Physics::CollisionComponent> collisionComponent;
		};

		struct DynamicObject : Object {
			DynamicObject(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent, const std::weak_ptr<Physics::MotionComponent>& motionComponent) :
				Object(collisionComponent), motionComponent(motionComponent) {}
			std::weak_ptr<Physics::MotionComponent> motionComponent;
		};
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
		static bool checkCollision(const BoundingBox& dynamicBox, const std::shared_ptr<Physics::MotionComponent>& dynamicMotionComponent, const BoundingBox& otherBox);
		static bool checkCollision(const std::shared_ptr<Physics::CollisionComponent>& dynamicObjectCollisionComponent, const std::shared_ptr<Physics::MotionComponent>& dynamicObjectMotionComponent, const std::shared_ptr<Physics::CollisionComponent>& otherCollisionComponent);
		static bool checkCollision(const Vec3<Length>& point, const BoundingBox& box);
		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);
		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		void addDynamicComponents(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent, const std::weak_ptr<Physics::MotionComponent>& motionComponent);
		void addObjectComponent(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent);
		void removeComponents(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent);

		void update() const;
	private:
		std::vector<DynamicObject> dynamicObjects;
		std::vector<Object> objects;
	};
}
