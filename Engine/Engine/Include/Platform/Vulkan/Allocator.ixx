export module gse.platform.allocator;

import std;
import vulkan_hpp;

struct memory_block;

export namespace gse::vulkan::allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
		memory_block* owner = nullptr;
	};

	auto initialize() -> void;
	auto shutdown() -> void;

	auto allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto free(allocation& alloc) -> void;
}

namespace gse::vulkan {
	
}