export module gse.platform.vulkan.transient_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.vulkan.resources;

export namespace gse::vulkan::transient_allocator {
	auto allocate(
		const config::device_config& config, 
		const vk::MemoryRequirements& requirements, 
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal
	) -> allocation;

	auto bind(
		config::device_config config, 
		vk::Buffer buffer, 
		const allocation& alloc
	) -> void;

	auto bind(
		config::device_config config, 
		vk::Image image, 
		const allocation& alloc
	) -> void;

	auto create_buffer(
		const config::device_config& config,
		const vk::BufferCreateInfo& buffer_info,
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		const void* data = nullptr
	) -> persistent_allocator::buffer_resource;

	auto end_frame() -> void;

	auto clean_up() -> void;
}
