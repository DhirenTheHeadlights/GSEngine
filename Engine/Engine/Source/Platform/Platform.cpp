module gse.platform;

import std;

import gse.platform.vulkan;
import gse.platform.glfw.window;

auto gse::platform::initialize() -> vulkan::config {
	window::initialize();
	return vulkan::initialize(window::get_window());
}

auto gse::platform::update() -> void {
	input::update();
}

auto gse::platform::shutdown(const vulkan::config& config) -> void {
	vulkan::shutdown(config);
	window::shutdown();
}