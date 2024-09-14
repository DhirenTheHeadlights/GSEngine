#pragma once

#include "Engine/Core/Object/Object.h"
#include "Engine/Physics/MotionComponent.h"

namespace Engine {
	class DynamicObject : public Object {
	public:
		DynamicObject(const std::shared_ptr<ID>& id) : Object(id) {}
		~DynamicObject() = default;

		///////////////////////////////////////////////////////////////////////////////////
		// Dynamic objects contain various components that are used to update the object //
		///////////////////////////////////////////////////////////////////////////////////

		Physics::MotionComponent& getMotionComponent() { return motionComponent; }
	protected:
		Physics::MotionComponent motionComponent;
	};
}
