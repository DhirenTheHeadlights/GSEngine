module;

#include <vulkan/vulkan.hpp>

module gse.platform.vulkan.objects;

auto gse::vulkan::get_memory_properties() -> vk::PhysicalDeviceMemoryProperties {
	return config::device::physical_device.getMemoryProperties();
}