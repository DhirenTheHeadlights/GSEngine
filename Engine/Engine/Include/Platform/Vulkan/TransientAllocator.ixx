export module gse.platform.vulkan.transient_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.vulkan.resources;

export namespace gse::vulkan::transient_allocator {
	auto allocate(config::device_config config, const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto bind(config::device_config config, vk::Buffer buffer, const allocation& alloc) -> void;
	auto bind(config::device_config config, vk::Image image, const allocation& alloc) -> void;
	auto end_frame() -> void;
}
