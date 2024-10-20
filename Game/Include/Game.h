#pragma once
#include "Engine/Engine.h"

namespace Game {
	bool initialize();
	bool update();
	bool render();
	bool close();

	const Engine::Camera& getCamera();
}
