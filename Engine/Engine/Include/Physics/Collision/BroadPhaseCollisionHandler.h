#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace Engine {
	class BroadPhaseCollisionHandler {
		struct RequiredComponents {
			RequiredComponents(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent, const std::weak_ptr<Physics::MotionComponent>& motionComponent) :
				collisionComponent(collisionComponent), motionComponent(motionComponent) {}
			std::weak_ptr<Physics::CollisionComponent> collisionComponent;
			std::weak_ptr<Physics::MotionComponent> motionComponent;
		};
	public:
		static bool checkCollision(const BoundingBox& box1, const BoundingBox& box2);
		static bool checkCollision(const BoundingBox& box1, const std::shared_ptr<Physics::MotionComponent>& box1Component, const BoundingBox& box2);
		static bool checkCollision(const std::shared_ptr<Physics::CollisionComponent>& object1, const std::shared_ptr<Physics::MotionComponent>& object1MotionComponent, const std::shared_ptr<Physics::CollisionComponent>& object2);
		static bool checkCollision(const Vec3<Length>& point, const BoundingBox& box);

		static CollisionInformation calculateCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		static void setCollisionInformation(const BoundingBox& box1, const BoundingBox& box2);

		void addComponents(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent, const std::weak_ptr<Physics::MotionComponent>& motionComponent) {
			components.emplace_back(collisionComponent, motionComponent);
		}

		void removeComponents(const std::weak_ptr<Physics::CollisionComponent>& collisionComponent) {
			std::erase_if(components, [&collisionComponent](const RequiredComponents& requiredComponents) {
				return requiredComponents.collisionComponent.lock() == collisionComponent.lock();
				});
		}

		void update() const;
	private:
		std::vector<RequiredComponents> components;
	};
}
