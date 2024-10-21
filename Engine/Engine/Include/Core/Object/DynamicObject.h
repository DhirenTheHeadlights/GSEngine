#pragma once

#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace Engine {
	class DynamicObject : public Object {
	public:
		explicit DynamicObject(const std::shared_ptr<ID>& id) : Object(id) {}
		~DynamicObject() override = default;

		///////////////////////////////////////////////////////////////////////////////////
		// Dynamic objects contain various components that are used to update the object //
		///////////////////////////////////////////////////////////////////////////////////

		Physics::MotionComponent& getMotionComponent() { return motionComponent; }
	protected:
		Physics::MotionComponent motionComponent;
	};
}
