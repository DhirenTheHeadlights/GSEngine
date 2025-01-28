export module gse.core.engine;

import std;

import gse.graphics.camera;

export namespace gse {
	auto initialize(const std::function<void()>& initialize_function, const std::function<void()>& shutdown_function) -> void;
	auto run(const std::function<bool()>& update_function, const std::function<bool()>& render_function) -> void;

	/// Request the engine to shut down after the current frame.
	auto request_shutdown() -> void;
	auto block_shutdown_requests() -> void;

	auto set_imgui_enabled(bool enabled) -> void;
	auto get_camera() -> camera&;
}
