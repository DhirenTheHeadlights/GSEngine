module gse.vulkan.transient_allocator;

import std;
import vulkan_hpp;

import gse.platform.context;
import gse.platform.assert;

struct memory_block {
	vk::DeviceMemory memory;
	vk::DeviceSize size, cursor = 0;
	vk::MemoryPropertyFlags properties;
	void* mapped = nullptr;
};

struct pool {
	std::uint32_t memory_type_index;
	std::vector<memory_block> blocks;
};