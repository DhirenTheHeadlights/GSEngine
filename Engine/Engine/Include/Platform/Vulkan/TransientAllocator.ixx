export module gse.vulkan.transient_allocator;

import std;
import vulkan_hpp;

export namespace gse::vulkan::transient_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
	};

	auto allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto end_frame() -> void;
}
