export module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;

struct sub_allocation;

export namespace gse::vulkan::persistent_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
		sub_allocation* owner = nullptr;
	};

	struct image_resource {
		vk::Image image;
		vk::ImageView view;
		allocation allocation;
	};

	struct buffer_resource {
		vk::Buffer buffer;
		allocation allocation;
	};

	auto allocate(config::device_config config, const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto bind(config::device_config config, vk::Buffer buffer, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto bind(config::device_config config, vk::Image image, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto create_buffer(config::device_config config, const vk::BufferCreateInfo& buffer_info, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, const void* data = nullptr) -> buffer_resource;
	auto create_image(config::device_config config, const vk::ImageCreateInfo& info, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageViewCreateInfo view_info = {}, const void* data = nullptr) -> image_resource;
	auto free(allocation& alloc) -> void;
	auto free(config::device_config config, buffer_resource& resource) -> void;
	auto free(config::device_config config, image_resource& resource) -> void;
}

namespace gse::vulkan::persistent_allocator {
	auto get_memory_flag_preferences(vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags>;
}