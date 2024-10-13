#pragma once
#include "Engine/Core/Engine.h"

namespace Game {
	bool initialize();
	bool update();
	void close();

	const Engine::Camera& getCamera();
}
