#pragma once

#include "Engine/Core/ID.h"
#include "Engine/Core/Object/Object.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Physics/BroadPhaseCollisionHandler.h"
#include "Object/DynamicObject.h"

namespace Engine {
	void initialize();
	void update(const Camera& camera);
	void render();
	void shutdown();

	void addObject(Object& object);
	void addObject(DynamicObject& object);

	void removeObject(Object& object);
	void removeObject(DynamicObject& object);

	extern IDHandler idManager;
	extern BroadPhaseCollisionHandler collisionHandler;
	extern Shader shader;
}
