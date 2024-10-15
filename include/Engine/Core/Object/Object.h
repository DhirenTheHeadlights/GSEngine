#pragma once

#include <vector>

#include "Engine/Core/ID.h"
#include "Engine/Graphics/BoundingBox.h"

namespace Engine {
	class Object {
	public:
		explicit Object(const std::shared_ptr<ID>& id) : id(id) {}
		virtual ~Object() = default;

		////////////////////////////////////////////////////////////////////////////////////
		/// All game objects require a colliding info and a vector of bounding boxes     ///
		/// These get passed into collision handler and are used to check for collisions ///
		////////////////////////////////////////////////////////////////////////////////////

		virtual bool isColliding() const { return colliding; }
		virtual void setIsColliding(const bool isColliding) { colliding = isColliding; }
		virtual std::vector<BoundingBox>& getBoundingBoxes() { return boundingBoxes; }

		ID* getId() const { return id.get(); }

		bool operator==(const Object& other) const {
			return id == other.id;
		}
	protected:
		std::vector<BoundingBox> boundingBoxes;
		bool colliding = false;

		std::shared_ptr<ID> id;
	};
}