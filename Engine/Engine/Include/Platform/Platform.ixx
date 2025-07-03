export module gse.platform;

export import :window;
export import :input;
export import :image_loader;

export import gse.platform.vulkan;

export namespace gse::platform {
	auto initialize() -> vulkan::config;
	auto update() -> void;
	auto shutdown() -> void;
}

auto gse::platform::initialize() -> vulkan::config {
	window::initialize();
	return vulkan::initialize(window::get_window());
}

auto gse::platform::update() -> void {
	input::update();
}

auto gse::platform::shutdown() -> void {
	vulkan::persistent_allocator::clean_up();
	window::shutdown();
}