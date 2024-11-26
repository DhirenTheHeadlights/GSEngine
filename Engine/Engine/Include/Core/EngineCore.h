#pragma once
#include <functional>
#include <memory>

#include "ID.h"
#include "SceneHandler.h"
#include "Graphics/Camera.h"
#include "Graphics/Renderer.h"
#include "Object/Object.h"

namespace Engine {
	void initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction);
	void run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction);

	/// Request the engine to shut down after the current frame.
	void requestShutdown();
	void blockShutdownRequests();

	void setImguiEnabled(bool enabled);
	Camera& getCamera();

	extern SceneHandler sceneHandler;
}
