export module gse.platform;

export import :window;
export import :input;
export import :image_loader;

export import gse.platform.vulkan;

export namespace gse::platform {
	auto initialize() -> vulkan::config;
	auto update() -> void;
	auto shutdown(const vulkan::config& config) -> void;
}

auto gse::platform::initialize() -> vulkan::config {
	window::initialize();
	return vulkan::initialize(window::get_window());
}

auto gse::platform::update() -> void {
	input::update();
}

auto gse::platform::shutdown(const vulkan::config& config) -> void {
	vulkan::persistent_allocator::clean_up(config.device_data.device);
	vulkan::shutdown(config);
	window::shutdown();
}