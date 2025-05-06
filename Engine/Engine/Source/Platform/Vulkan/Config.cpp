module gse.platform.vulkan.config;
module;

#include <vulkan/vulkan.hpp>

module gse.platform.vulkan.objects;

import vulkan_hpp;

import gse.platform.vulkan.context;
auto gse::vulkan::get_memory_properties() -> vk::PhysicalDeviceMemoryProperties {
	return config::device::physical_device.getMemoryProperties();
}