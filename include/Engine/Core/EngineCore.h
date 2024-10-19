#pragma once
#include <functional>
#include "Engine/Core/ID.h"
#include "Engine/Core/Object/Object.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Object/DynamicObject.h"
#include "Object/StaticObject.h"

namespace Engine {
	void initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction);
	void update(const std::function<bool()>& updateFunction);
	void render(const Camera& camera, const std::function<bool()>& renderFunction);
	void shutdown();

	void addObject(const std::weak_ptr<StaticObject>& object);
	void addObject(const std::weak_ptr<DynamicObject>& object);

	void removeObject(const std::weak_ptr<StaticObject>& object);
	void removeObject(const std::weak_ptr<DynamicObject>& object);

	extern IDHandler idManager;
}
