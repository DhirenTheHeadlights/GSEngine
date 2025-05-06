module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.context;
import gse.platform.vulkan.config;
import gse.platform.assert;

struct sub_allocation {
	vk::DeviceSize offset;
	vk::DeviceSize size;
	bool in_use = false;
};

struct memory_block {
	vk::DeviceMemory memory;
	vk::DeviceSize size;
	vk::MemoryPropertyFlags properties;
	std::vector<sub_allocation> allocations;
	void* mapped = nullptr;
};

struct pool {
	std::uint32_t memory_type_index;
	std::vector<memory_block> blocks;
};

constexpr vk::DeviceSize g_default_block_size = 1024 * 1024 * 64; // 64 MB

std::unordered_map<std::uint32_t, pool> g_memory_pools;

auto gse::vulkan::persistent_allocator::initialize() -> void {
	for (auto& [_, blocks] : g_memory_pools | std::views::values) {
		for (const auto& block : blocks) {
			if (block.mapped) {
				config::device::device.unmapMemory(block.memory);
			}
			config::device::device.freeMemory(block.memory);
		}
	}
	g_memory_pools.clear();
}

auto gse::vulkan::persistent_allocator::shutdown() -> void {
	
}

auto gse::vulkan::persistent_allocator::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = config::device::physical_device.getMemoryProperties();
	auto memory_type_index = std::numeric_limits<std::uint32_t>::max();

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if (requirements.memoryTypeBits & 1 << i && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			memory_type_index = i;
			break;
		}
	}

	assert(memory_type_index != std::numeric_limits<std::uint32_t>::max(), std::format("Failed to find suitable memory type for allocation!"));

	for (auto pool : g_memory_pools | std::views::values) {
		if (pool.memory_type_index != memory_type_index) {
			continue;
		}

		for (auto& block : pool.blocks) {
			if (block.properties != properties) {
				continue;
			}

			for (auto& sub : block.allocations) {
				if (sub.in_use || sub.size <= requirements.size) {
					continue;
				}

				if (const vk::DeviceSize aligned_offset = sub.offset + requirements.alignment - 1 & ~(requirements.alignment - 1); aligned_offset + requirements.size <= sub.offset + sub.size) {
					sub.in_use = true;
					return {
						.memory = block.memory,
						.size = requirements.size,
						.offset = aligned_offset,
						.mapped = block.mapped ? static_cast<char*>(block.mapped) + aligned_offset : nullptr,
						.owner = &sub
					};
				}
			}
		}
	}

	// No suitable block found, create a new one
	auto& new_block = g_memory_pools[memory_type_index].blocks.emplace_back();
	new_block.size = std::max(requirements.size, g_default_block_size);
	new_block.properties = properties;

	new_block.memory = config::device::device.allocateMemory(
		vk::MemoryAllocateInfo{
			new_block.size,
			memory_type_index
		}
	);

	return allocate(requirements, properties); 
}

auto gse::vulkan::persistent_allocator::bind(vk::Buffer buffer, const allocation& alloc) -> void {
	config::device::device.bindBufferMemory(buffer, alloc.memory, alloc.offset);
}

auto gse::vulkan::persistent_allocator::bind(vk::Image image, const allocation& alloc) -> void {
	config::device::device.bindImageMemory(image, alloc.memory, alloc.offset);
}

auto gse::vulkan::persistent_allocator::free(allocation& alloc) -> void {
	if (alloc.owner) {
		alloc.owner->in_use = false;
		alloc.owner = nullptr;
	}
}
