#pragma once

#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace Engine {
	class DynamicObject : public Object {
	public:
		explicit DynamicObject(const std::shared_ptr<ID>& id) : Object(id) {}
		~DynamicObject() override = default;

		///////////////////////////////////////////////////////////////////////////////////
		// Dynamic objectMotionComponents contain various components that are used to update the object //
		///////////////////////////////////////////////////////////////////////////////////

		std::shared_ptr<Physics::MotionComponent>& getMotionComponent() { return motionComponent; }
	protected:
		std::shared_ptr<Physics::MotionComponent> motionComponent = std::make_shared<Physics::MotionComponent>();
	};
}
