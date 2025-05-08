export module gse.platform;

export import gse.platform.glfw.window;
export import gse.platform.glfw.input;
export import gse.platform.assert;
export import gse.platform.vulkan;

export namespace gse::platform {
	auto initialize() -> void;
	auto update() -> void;
	auto shutdown() -> void;
}