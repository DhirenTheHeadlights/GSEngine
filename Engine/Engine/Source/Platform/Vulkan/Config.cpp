module;

#include <vulkan/vulkan.hpp>

module gse.platform.vulkan.config;

auto gse::vulkan::get_memory_properties() -> vk::PhysicalDeviceMemoryProperties {
	return config::device::physical_device.getMemoryProperties();
}