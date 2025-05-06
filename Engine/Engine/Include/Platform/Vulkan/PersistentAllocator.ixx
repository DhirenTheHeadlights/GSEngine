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

	auto initialize() -> void;
	auto shutdown() -> void;

	auto allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto bind(vk::Buffer buffer, const allocation& alloc) -> void;
	auto bind(vk::Image image, const allocation& alloc) -> void;
	auto free(allocation& alloc) -> void;
}