module gse.platform;

import gse.platform.vulkan;
import gse.platform.glfw.window;

auto gse::platform::initialize() -> void {
	window::initialize();
	vulkan::initialize(window::get_window());
}

auto gse::platform::update() -> void {
	input::update();
}

auto gse::platform::shutdown() -> void {
	vulkan::shutdown();
	window::shutdown();
}