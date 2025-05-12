module;

#include <vulkan/vulkan.hpp>

module gse.platform.vulkan.config;

auto gse::vulkan::get_memory_properties(const vk::PhysicalDevice device) -> vk::PhysicalDeviceMemoryProperties {
	return device.getMemoryProperties();
}