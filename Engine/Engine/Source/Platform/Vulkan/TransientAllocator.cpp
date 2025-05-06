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

constexpr vk::DeviceSize g_default_block_size = 64 * 1024 * 1024;

std::unordered_map<std::uint32_t, pool> g_memory_pools;

auto align_up(const vk::DeviceSize offset, const vk::DeviceSize alignment) -> vk::DeviceSize {
	return offset + alignment - 1 & ~(alignment - 1);
}

auto gse::vulkan::transient_allocator::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = get_memory_properties();
	auto req_memory_type_index = std::numeric_limits<std::uint32_t>::max();

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if (requirements.memoryTypeBits & 1 << i && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			req_memory_type_index = i;
			break;
		}
	}

	assert(req_memory_type_index != std::numeric_limits<std::uint32_t>::max(), std::format("Failed to find suitable memory type for allocation!"));

	auto& [memory_type_index, blocks] = g_memory_pools[req_memory_type_index];

	for (auto& [memory, size, cursor, block_properties, mapped] : blocks) {
		if (block_properties != properties) {
			continue;
		}

		if (const vk::DeviceSize aligned_offset = align_up(cursor, requirements.alignment); aligned_offset + requirements.size <= size) {
			cursor = aligned_offset + requirements.size;

			return {
				.memory = memory,
				.size = requirements.size,
				.offset = aligned_offset,
				.mapped = mapped ? static_cast<std::byte*>(mapped) + aligned_offset : nullptr
			};
		}
	}

	// Allocate a new memory block
	const vk::DeviceMemory memory = config::device::device.allocateMemory({
		g_default_block_size,
		memory_type_index
		});

	void* mapped = nullptr;
	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		mapped = config::device::device.mapMemory(memory, 0, g_default_block_size);
	}

	memory_block new_block{
		.memory = memory,
		.size = g_default_block_size,
		.cursor = requirements.size,
		.properties = properties,
		.mapped = mapped
	};

	blocks.push_back(std::move(new_block));
	const auto& block = blocks.back();

	return {
		.memory = block.memory,
		.size = requirements.size,
		.offset = 0,
		.mapped = static_cast<std::byte*>(block.mapped)
	};
}

auto gse::vulkan::transient_allocator::end_frame() -> void {
	for (auto& [memory_type_index, blocks] : g_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			block.cursor = 0;
		}
	}
}
