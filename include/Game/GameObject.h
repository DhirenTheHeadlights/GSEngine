#pragma once

#include <vector>

#include "ID.h"
#include "CollisionHandler.h"

namespace Game {
	class GameObject {
	public:
		GameObject(const std::shared_ptr<Engine::ID>& id) : id(id) {}
		virtual ~GameObject() = default;

		////////////////////////////////////////////////////////////////////////////////////
		/// All game objects require a colliding info and a vector of bounding boxes     ///
		/// These get passed into collision handler and are used to check for collisions ///
		////////////////////////////////////////////////////////////////////////////////////

		virtual bool isColliding() const { return colliding; }
		virtual void setIsColliding(const bool isColliding) { colliding = isColliding; }
		virtual std::vector<Engine::BoundingBox>& getBoundingBoxes() { return boundingBoxes; }

		Engine::ID* getId() const { return id.get(); }

		bool operator==(const GameObject& other) const {
			return id == other.id;
		}
	protected:
		std::vector<Engine::BoundingBox> boundingBoxes;
		bool colliding = false;

		std::shared_ptr<Engine::ID> id;
	};
}