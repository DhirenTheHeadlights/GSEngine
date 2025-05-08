export module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

struct sub_allocation;

export namespace gse::vulkan::persistent_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
		sub_allocation* owner = nullptr;
	};

	auto allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto bind(vk::Buffer buffer, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto bind(vk::Image image, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, const void* data = nullptr) -> std::pair<vk::Buffer, allocation>;
	auto create_image(const vk::ImageCreateInfo& info, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, const void* data = nullptr) -> std::pair<vk::Image, allocation>;
	auto free(allocation& alloc) -> void;
}