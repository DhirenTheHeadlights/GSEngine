#pragma once

#include "Engine/Core/ID.h"
#include "Engine/Core/Object/Object.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Object/DynamicObject.h"
#include "Object/StaticObject.h"

namespace Engine {
	void initialize();
	void update(const Camera& camera);
	void render();
	void shutdown();

	void addObject(const std::weak_ptr<StaticObject>& object);
	void addObject(const std::weak_ptr<DynamicObject>& object);

	void removeObject(const std::weak_ptr<StaticObject>& object);
	void removeObject(const std::weak_ptr<DynamicObject>& object);

	extern IDHandler idManager;
	extern BroadPhaseCollisionHandler collisionHandler;
	extern Shader shader;
}
