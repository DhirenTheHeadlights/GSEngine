module gse.platform.vulkan.transient_allocator;

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
	vk::DeviceSize size, cursor = 0;
	vk::MemoryPropertyFlags properties;
	std::vector<sub_allocation> allocations;
	void* mapped = nullptr;
};

struct pool {
	std::uint32_t memory_type_index;
	std::vector<memory_block> blocks;
};

constexpr vk::DeviceSize g_transient_default_block_size = 64 * 1024 * 1024;

std::unordered_map<std::uint32_t, pool> g_transient_memory_pools;

auto align_up(const vk::DeviceSize offset, const vk::DeviceSize alignment) -> vk::DeviceSize {
	return offset + alignment - 1 & ~(alignment - 1);
}

auto gse::vulkan::transient_allocator::allocate(const config::device_config config, const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = get_memory_properties(config.physical_device);
	auto req_memory_type_index = std::numeric_limits<std::uint32_t>::max();

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if (requirements.memoryTypeBits & 1 << i && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			req_memory_type_index = i;
			break;
		}
	}

	assert(req_memory_type_index != std::numeric_limits<std::uint32_t>::max(), std::format("Failed to find suitable memory type for allocation!"));

	auto& [memory_type_index, blocks] = g_transient_memory_pools[req_memory_type_index];
	if (blocks.empty()) {
		memory_type_index = req_memory_type_index;
	}

	memory_block* best_block = nullptr;
	std::size_t best_index = std::numeric_limits<std::size_t>::max();
	vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
	vk::DeviceSize aligned_offset = 0;

	for (auto& block : blocks) {
		if (block.properties != properties) continue;

		for (std::size_t i = 0; i < block.allocations.size(); ++i) {
			const auto& [offset, size, in_use] = block.allocations[i];
			if (in_use) continue;

			const vk::DeviceSize aligned = align_up(offset, requirements.alignment);
			if (aligned + requirements.size > offset + size) continue;

			if (const vk::DeviceSize leftover = size - (aligned - offset) - requirements.size; leftover < best_leftover) {
				best_block = &block;
				best_index = i;
				best_leftover = leftover;
				aligned_offset = aligned;
				if (leftover == 0) break; // exact fit
			}
		}
		if (best_leftover == 0) break;
	}

	if (best_block) {
		auto& vec = best_block->allocations;
		const auto sub = vec[best_index];

		const vk::DeviceSize prefix = aligned_offset - sub.offset;
		const vk::DeviceSize suffix = sub.size - prefix - requirements.size;

		vec[best_index] = { aligned_offset, requirements.size, true };
		std::size_t insert_at = best_index;

		if (suffix) {
			vec.insert(vec.begin() + ++insert_at, { aligned_offset + requirements.size, suffix, false });
		}
		if (prefix) {
			vec.insert(vec.begin() + best_index, { sub.offset, prefix, false });
		}

		return {
			best_block->memory,
			requirements.size,
			aligned_offset,
			best_block->mapped
				? static_cast<std::byte*>(best_block->mapped) + aligned_offset
				: nullptr
		};
	}

	vk::DeviceSize size = std::max(requirements.size, g_transient_default_block_size);
	auto& new_block = blocks.emplace_back(
		memory_block{
			config.device.allocateMemory({ size, req_memory_type_index }),
			size,
			0,
			properties,
			{},
			nullptr
		}
	);

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = config.device.mapMemory(new_block.memory, 0, new_block.size);
	}

	new_block.allocations.clear();
	new_block.allocations.push_back({ 0, new_block.size, false });

	const vk::DeviceSize new_aligned_offset = align_up(0, requirements.alignment);
	const vk::DeviceSize prefix = new_aligned_offset;
	const vk::DeviceSize suffix = new_block.size - prefix - requirements.size;

	new_block.allocations.clear();

	if (prefix) {
		new_block.allocations.push_back({ 0, prefix, false });
	}

	new_block.allocations.push_back({ new_aligned_offset, requirements.size, true });

	if (suffix) {
		new_block.allocations.push_back({ new_aligned_offset + requirements.size, suffix, false });
	}

	return {
		new_block.memory,
		requirements.size,
		new_aligned_offset,
		new_block.mapped
			? static_cast<std::byte*>(new_block.mapped) + new_aligned_offset
			: nullptr
	};
}

auto gse::vulkan::transient_allocator::bind(const config::device_config config, const vk::Buffer buffer, const allocation& alloc) -> void {
	config.device.bindBufferMemory(buffer, alloc.memory, alloc.offset);
}

auto gse::vulkan::transient_allocator::bind(const config::device_config config, const vk::Image image, const allocation& alloc) -> void {
	config.device.bindImageMemory(image, alloc.memory, alloc.offset);
}

auto gse::vulkan::transient_allocator::end_frame() -> void {
	for (auto& [memory_type_index, blocks] : g_transient_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			block.cursor = 0;
		}
	}
}
