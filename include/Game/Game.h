#pragma once
#include "Engine/Core/Engine.h"

namespace Game {
	bool initialize();
	bool update();
	bool render();
	bool close();

	const Engine::Camera& getCamera();
}
