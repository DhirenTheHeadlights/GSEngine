#pragma once
#include <functional>
#include <memory>

#include "ID.h"
#include "Graphics/Camera.h"
#include "Graphics/Renderer.h"
#include "Object/Object.h"

namespace Engine {
	void initialize(const std::function<void()>& initializeFunction, const std::function<void()>& shutdownFunction);
	void run(const std::function<bool()>& updateFunction, const std::function<bool()>& renderFunction);

	void addObject(const std::weak_ptr<Object>& object);
	void removeObject(const std::weak_ptr<Object>& object);

	/// Request the engine to shut down after the current frame.
	void requestShutdown();

	Camera& getCamera();

	extern IDHandler idManager;
}
