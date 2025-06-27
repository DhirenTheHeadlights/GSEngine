export module gse.platform.vulkan:transient_allocator;

import std;

import :config;
import :resources;

import gse.utility;

export namespace gse::vulkan::transient_allocator {
	auto allocate(
		const config::device_config& config, 
		const vk::MemoryRequirements& requirements, 
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal
	) -> allocation;

	auto bind(
		config::device_config config, 
		vk::Buffer buffer, 
		const allocation& alloc
	) -> void;

	auto bind(
		config::device_config config, 
		vk::Image image, 
		const allocation& alloc
	) -> void;

	auto create_buffer(
		const config::device_config& config,
		const vk::BufferCreateInfo& buffer_info,
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		const void* data = nullptr
	) -> persistent_allocator::buffer_resource;

	auto end_frame() -> void;

	auto clean_up() -> void;
}

struct memory_block {
	vk::raii::DeviceMemory memory = nullptr;
	vk::DeviceSize size, cursor = 0;
	vk::MemoryPropertyFlags properties;
	void* mapped = nullptr;
};

struct pool {
	std::uint32_t memory_type_index;
	std::vector<memory_block> blocks;
};

constexpr vk::DeviceSize g_transient_default_block_size = 64 * 1024 * 1024; // 64 MB

std::unordered_map<std::uint32_t, pool> g_transient_memory_pools;

auto align_up(const vk::DeviceSize offset, const vk::DeviceSize alignment) -> vk::DeviceSize {
	return (offset + alignment - 1) & ~(alignment - 1);
}

auto gse::vulkan::transient_allocator::allocate(const config::device_config& config, const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = get_memory_properties(*config.physical_device);
	auto req_memory_type_index = std::numeric_limits<std::uint32_t>::max();

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if ((requirements.memoryTypeBits & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			req_memory_type_index = i;
			break;
		}
	}

	assert(req_memory_type_index != std::numeric_limits<std::uint32_t>::max(), "Failed to find suitable memory type!");

	auto& pool = g_transient_memory_pools[req_memory_type_index];
	if (pool.blocks.empty()) {
		pool.memory_type_index = req_memory_type_index;
	}

	for (auto& block : pool.blocks) {
		if (block.properties != properties) continue;

		if (const vk::DeviceSize aligned_offset = align_up(block.cursor, requirements.alignment); aligned_offset + requirements.size <= block.size) {
			block.cursor = aligned_offset + requirements.size;
			return { *block.memory, requirements.size, aligned_offset, block.mapped ? static_cast<std::byte*>(block.mapped) + aligned_offset : nullptr };
		}
	}

	vk::DeviceSize new_block_size = std::max(requirements.size, g_transient_default_block_size);
	auto& new_block = pool.blocks.emplace_back();
	new_block.properties = properties;
	new_block.size = new_block_size;
	new_block.memory = config.device.allocateMemory({
		.allocationSize = new_block_size,
		.memoryTypeIndex = req_memory_type_index
		});

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = new_block.memory.mapMemory(0, new_block.size);
	}

	const vk::DeviceSize aligned_offset = align_up(0, requirements.alignment);
	assert(aligned_offset + requirements.size <= new_block.size, "New block is too small!");
	new_block.cursor = aligned_offset + requirements.size;

	return { *new_block.memory, requirements.size, aligned_offset, new_block.mapped ? static_cast<std::byte*>(new_block.mapped) + aligned_offset : nullptr };
}

auto gse::vulkan::transient_allocator::create_buffer(const config::device_config& config, const vk::BufferCreateInfo& buffer_info, const vk::MemoryPropertyFlags properties, const void* data) -> persistent_allocator::buffer_resource {
	vk::raii::Buffer buffer = config.device.createBuffer(buffer_info);

	const vk::BufferMemoryRequirementsInfo2 info{
		.buffer = *buffer
	};
	const auto requirements = config.device.getBufferMemoryRequirements2(info).memoryRequirements;

	const auto alloc = allocate(config, requirements, properties);

	buffer.bindMemory(alloc.memory, alloc.offset);

	if (data && alloc.mapped) {
		std::memcpy(alloc.mapped, data, buffer_info.size);
	}

	return { std::move(buffer), { alloc.memory, alloc.size, alloc.offset, alloc.mapped, nullptr } };
}

auto gse::vulkan::transient_allocator::end_frame() -> void {
	for (auto& [memory_type_index, blocks] : g_transient_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			block.cursor = 0;
		}
	}
}

auto gse::vulkan::transient_allocator::clean_up() -> void {
	for (auto& [memory_type_index, blocks] : g_transient_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			if (block.mapped) {
				block.memory.unmapMemory();
				block.mapped = nullptr;
			}
		}
		blocks.clear();
	}
	g_transient_memory_pools.clear();
}