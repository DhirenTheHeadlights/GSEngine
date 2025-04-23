module gse.platform.allocator;

import std;
import vulkan_hpp;

import gse.platform.context;
import gse.platform.assert;

struct memory_block {
	vk::DeviceMemory memory;
	vk::DeviceSize size;
	vk::DeviceSize offset;
	vk::DeviceSize alignment;
	void* mapped = nullptr;
	bool in_use = false;
};

std::unordered_map<std::uint32_t, std::vector<memory_block>> g_memory_blocks;

auto gse::vulkan::allocator::initialize() -> void {
	
}

auto gse::vulkan::allocator::shutdown() -> void {
	for (auto& blocks : g_memory_blocks | std::views::values) {
		for (const auto& block : blocks) {
			const auto& device = get_device_config().device;
			if (block.mapped) {
				device.unmapMemory(block.memory);
			}
			device.freeMemory(block.memory);
		}
	}
}

auto gse::vulkan::allocator::allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = get_device_config().physical_device.getMemoryProperties();
	std::uint32_t memory_type_index = 0;

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if ((requirements.memoryTypeBits & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			memory_type_index = i;
			break;
		}
	}

	assert(memory_type_index != 0, std::format("Failed to find suitable memory type for allocation!"));

	for (auto& block : g_memory_blocks[memory_type_index]) {
		if (!block.in_use && block.size >= requirements.size && block.alignment >= requirements.alignment) {
			block.in_use = true;
			block.offset += requirements.size;

			return {
				.memory = block.memory,
				.size = requirements.size,
				.offset = block.offset,
				.mapped = block.mapped,
				.owner = &block
			};
		}
	}

	const vk::MemoryAllocateInfo alloc_info(
		requirements.size,
		memory_type_index
	);

	vk::DeviceMemory memory = get_device_config().device.allocateMemory(alloc_info);
	void* mapped = nullptr;
	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		mapped = get_device_config().device.mapMemory(memory, 0, requirements.size);
	}

	g_memory_blocks[memory_type_index].emplace_back(
		memory,
		requirements.size,
		0,
		requirements.alignment,
		mapped,
		true
	);

	return {
		.memory = memory,
		.size = requirements.size,
		.offset = 0,
		.mapped = mapped,
		.owner = &g_memory_blocks[memory_type_index].back()
	};
}

auto gse::vulkan::allocator::free(allocation& alloc) -> void {
	if (alloc.owner) {
		alloc.owner->in_use = false;
		alloc.owner = nullptr;
	}
}
