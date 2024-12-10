#pragma once
#include <functional>
#include <memory>

#include "ID.h"
#include "SceneHandler.h"
#include "Graphics/3D/Camera.h"
#include "Graphics/3D/Renderer3D.h"
#include "Object/Object.h"

namespace gse {
	void initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function);
	void run(const std::function<bool()>& update_function, const std::function<bool()>& render_function);

	/// Request the engine to shut down after the current frame.
	void request_shutdown();
	void block_shutdown_requests();

	void set_imgui_enabled(bool enabled);
	Camera& get_camera();

	extern SceneHandler scene_handler;
}
