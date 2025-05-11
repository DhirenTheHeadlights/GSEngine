export module gse.platform;

export import gse.platform.glfw.window;
export import gse.platform.glfw.input;
export import gse.platform.assert;
export import gse.platform.vulkan;
export import gse.platform.stb.image_loader;

export namespace gse::platform {
	auto initialize() -> vulkan::config;
	auto update() -> void;
	auto shutdown(const vulkan::config& config) -> void;
}