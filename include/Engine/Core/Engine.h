#pragma once

#include "Engine/Core/ID.h"
#include "Engine/Core/Object/Object.h"
#include "Object/DynamicObject.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Physics/CollisionHandler.h"
#include "Engine/Physics/System.h"

namespace Engine {
	void initialize();
	void update(float deltaTime, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model);
	void render();
	void shutdown();

	void addObject(Object& object);
	void addObject(DynamicObject& object);

	void removeObject(Object& object);
	void removeObject(DynamicObject& object);

	extern IDHandler idManager;
	extern CollisionHandler collisionHandler;
	extern Shader shader;
}
