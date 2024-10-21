#pragma once
#include <functional>
#include "Core/ID.h"
#include "Graphics/Camera.h"
#include "Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Object/DynamicObject.h"
#include "Object/StaticObject.h"

namespace Engine {
	void initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction);
	void run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction);

	void addObject(const std::weak_ptr<StaticObject>& object);
	void addObject(const std::weak_ptr<DynamicObject>& object);

	void removeObject(const std::weak_ptr<StaticObject>& object);
	void removeObject(const std::weak_ptr<DynamicObject>& object);

	Camera& getCamera();

	extern IDHandler idManager;
}
