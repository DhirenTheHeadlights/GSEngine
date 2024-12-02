#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace Engine::Collisions {
	class CollisionGroup {
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
		void addObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent);
		void removeObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent);

		void addDynamicObject(const std::shared_ptr<Physics::CollisionComponent>& collisionComponent, const std::shared_ptr<Physics::MotionComponent>& motionComponent);

		const std::vector<DynamicObject>& getDynamicObjects() const { return dynamicObjects; }
		const std::vector<Object>& getObjects() const { return objects; }
	private:
		std::vector<DynamicObject> dynamicObjects;
		std::vector<Object> objects;
	};
}

namespace Engine::BroadPhaseCollisions {
	bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
	bool checkCollision(const BoundingBox& dynamicBox, const std::shared_ptr<Physics::MotionComponent>& dynamicMotionComponent, const BoundingBox& otherBox);
	bool checkCollision(const std::shared_ptr<Physics::CollisionComponent>& dynamicObjectCollisionComponent, const std::shared_ptr<Physics::MotionComponent>& dynamicObjectMotionComponent, const std::shared_ptr<Physics::CollisionComponent>& otherCollisionComponent);
	bool checkCollision(const Vec3<Length>& point, const BoundingBox& box);
	CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);
	void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

	void update(const Collisions::CollisionGroup& group);
}
