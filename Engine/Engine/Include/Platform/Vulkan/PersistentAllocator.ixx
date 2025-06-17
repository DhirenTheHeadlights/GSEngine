export module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.assert;
import gse.platform.vulkan.resources;

export namespace gse::vulkan::persistent_allocator {
	auto allocate(
		const config::device_config& config, 
		const vk::MemoryRequirements& requirements, 
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal
	) -> allocation;

	auto create_buffer(
		const config::device_config& config, 
		const vk::BufferCreateInfo& buffer_info, 
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, 
		const void* data = nullptr
	) -> buffer_resource;

	auto create_image(
		const config::device_config& config, 
		const vk::ImageCreateInfo& info, 
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, 
		vk::ImageViewCreateInfo view_info = {}, const void* data = nullptr
	) -> image_resource;

	auto clean_up(vk::Device device) -> void;
}

namespace gse::vulkan::persistent_allocator {
	auto free(
		allocation& alloc
	) -> void;

	auto get_memory_flag_preferences(
		vk::BufferUsageFlags usage
	) -> std::vector<vk::MemoryPropertyFlags>;
}