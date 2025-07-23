export module gse.platform;

export import :window;
export import :input;
export import :image_loader;

export import gse.platform.vulkan;

export namespace gse::platform {
	auto initialize() -> std::unique_ptr<vulkan::config>;
	auto update() -> void;
	auto shutdown() -> void;
}

auto gse::platform::initialize() -> std::unique_ptr<vulkan::config> {
	window::initialize();
	input::set_up_key_maps();
	return vulkan::initialize(window::get_window());
}

auto gse::platform::update() -> void {
	input::update();
}

auto gse::platform::shutdown() -> void {
	vulkan::persistent_allocator::clean_up();
	window::shutdown();
}