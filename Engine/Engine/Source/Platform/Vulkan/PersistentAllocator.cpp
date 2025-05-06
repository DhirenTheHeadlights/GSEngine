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
	const auto mem_properties = get_memory_properties();
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

auto gse::vulkan::persistent_allocator::bind(const vk::Buffer buffer, const vk::MemoryPropertyFlags properties, vk::MemoryRequirements requirements) -> allocation {
	if (requirements.size == 0) {
		requirements = config::device::device.getBufferMemoryRequirements(buffer);
	}

	const auto& allocation = allocate(requirements, properties);
	config::device::device.bindBufferMemory(buffer, allocation.memory, allocation.offset);
	return allocation;
}

auto gse::vulkan::persistent_allocator::bind(const vk::Image image, const vk::MemoryPropertyFlags properties, vk::MemoryRequirements requirements) -> allocation {
	if (requirements.size == 0) {
		requirements = config::device::device.getImageMemoryRequirements(image);
	}

	const auto& allocation = allocate(requirements, properties);
	config::device::device.bindImageMemory(image, allocation.memory, allocation.offset);
	return allocation;
}

auto gse::vulkan::persistent_allocator::create_buffer(const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, const void* data) -> std::pair<vk::Buffer, allocation> {
	const vk::BufferCreateInfo buffer_info{
		{},
		size,
		usage
	};

	auto buffer = config::device::device.createBuffer(buffer_info);
	const auto requirements = config::device::device.getBufferMemoryRequirements(buffer);
	auto alloc = bind(buffer, properties, requirements);

	if (data && alloc.mapped) {
		std::memcpy(alloc.mapped, data, size);
	}

	return { buffer, alloc };
}

auto gse::vulkan::persistent_allocator::create_image(const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, const void* data) -> std::pair<vk::Image, allocation> {
	auto image = config::device::device.createImage(info);

	const auto requirements = config::device::device.getImageMemoryRequirements(image);

	auto alloc = bind(image, properties, requirements);
	if (data && alloc.mapped) {
		std::memcpy(alloc.mapped, data, config::swap_chain::extent.width * config::swap_chain::extent.height * 4); // Assuming 4 bytes per pixel
	}

	return { image, alloc };
}

auto gse::vulkan::persistent_allocator::free(allocation& alloc) -> void {
	if (alloc.owner) {
		alloc.owner->in_use = false;
		alloc.owner = nullptr;
	}
}
