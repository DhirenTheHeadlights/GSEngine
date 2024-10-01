#pragma once
#include "Engine/Graphics/Camera.h"

namespace Game {
	bool initializeGame();
	bool gameLogic();
	void closeGame();

	const Engine::Camera& getCamera();
}
