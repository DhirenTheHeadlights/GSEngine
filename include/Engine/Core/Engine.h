#pragma once

#include "ID.h"
#include "Shader.h"
#include "CollisionHandler.h"

#include "GameObject.h"

namespace Engine {
	void initialize();
	void update(float deltaTime);
	void render();
	void shutdown();

	extern IDHandler idManager;
	extern CollisionHandler<GameObject> collisionHandler;
	extern Shader shader;
}