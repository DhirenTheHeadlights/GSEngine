#pragma once
#include "Engine/Graphics/Camera.h"

namespace Game {
	bool initialize();
	bool update();
	void close();

	const Engine::Camera& getCamera();
}
