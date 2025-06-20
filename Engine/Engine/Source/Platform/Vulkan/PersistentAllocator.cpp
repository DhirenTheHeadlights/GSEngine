module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.resources;
import gse.platform.vulkan.context;
import gse.platform.vulkan.config;
import gse.platform.assert;

struct memory_block {
	vk::raii::DeviceMemory memory;
	vk::DeviceSize size;
	vk::MemoryPropertyFlags properties;
	std::list<gse::vulkan::persistent_allocator::sub_allocation> allocations;
	void* mapped = nullptr;
};

struct pool {
	std::uint32_t memory_type_index;
	std::deque<memory_block> blocks;
};

struct pool_key {
	std::uint32_t memory_type_index;
	vk::MemoryPropertyFlags properties;

	auto operator==(const pool_key& other) const -> bool {
		return memory_type_index == other.memory_type_index && properties == other.properties;
	}
};

template <>
struct std::hash<pool_key> {
	auto operator()(const pool_key& key) const noexcept -> std::size_t {
		return std::hash<std::uint32_t>()(key.memory_type_index) ^ std::hash<vk::MemoryPropertyFlags>()(key.properties);
	}
};

constexpr vk::DeviceSize g_persistent_default_block_size = 1024 * 1024 * 64; // 64 MB

std::unordered_map<pool_key, pool> g_persistent_memory_pools;

auto gse::vulkan::persistent_allocator::allocate(const config::device_config& config, const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_props = get_memory_properties(config.physical_device);

	std::uint32_t memory_type_index = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((requirements.memoryTypeBits & (1u << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			memory_type_index = i;
			break;
		}
	}

	assert(
		memory_type_index != std::numeric_limits<std::uint32_t>::max(),
		std::format("Failed to find suitable memory type for allocation!")
	);

	auto& pool = g_persistent_memory_pools[{ memory_type_index, properties }];
	if (pool.blocks.empty()) pool.memory_type_index = memory_type_index;

	memory_block* best_block = nullptr;
	std::list<sub_allocation>::iterator best_sub_it;
	vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
	vk::DeviceSize best_aligned_offset = 0;

	for (auto& block : pool.blocks) {
		for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
			if (it->in_use) continue;

			const vk::DeviceSize aligned_offset = (it->offset + requirements.alignment - 1) & ~(requirements.alignment - 1);
			if (aligned_offset + requirements.size > it->offset + it->size) continue;

			const vk::DeviceSize leftover = it->size - (aligned_offset - it->offset) - requirements.size;
			if (leftover < best_leftover) {
				best_block = &block;
				best_sub_it = it;
				best_leftover = leftover;
				best_aligned_offset = aligned_offset;
				if (leftover == 0) break;
			}
		}
		if (best_leftover == 0) break;
	}

	if (best_block) {
		auto& list = best_block->allocations;
		const auto sub = *best_sub_it;

		const vk::DeviceSize prefix_size = best_aligned_offset - sub.offset;
		const vk::DeviceSize suffix_size = sub.size - prefix_size - requirements.size;

		const auto next_it = list.erase(best_sub_it);
		if (prefix_size) list.insert(next_it, { sub.offset, prefix_size, false });
		const auto owner_it = list.insert(next_it, { best_aligned_offset, requirements.size, true });
		if (suffix_size) list.insert(next_it, { best_aligned_offset + requirements.size, suffix_size, false });

		return {
			best_block->memory,
			requirements.size,
			best_aligned_offset,
			best_block->mapped ? static_cast<char*>(best_block->mapped) + best_aligned_offset : nullptr,
			&*owner_it
		};
	}

	auto& new_block = pool.blocks.emplace_back(nullptr);
	new_block.size = std::max(requirements.size, g_persistent_default_block_size);
	new_block.properties = properties;
	new_block.memory = config.device.allocateMemory({ 
		.allocationSize = new_block.size,
		.memoryTypeIndex = memory_type_index
	});

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = new_block.memory.mapMemory(0, new_block.size);
	}

	const vk::DeviceSize aligned_offset = (requirements.alignment - 1) & ~(requirements.alignment - 1);
	const vk::DeviceSize prefix_size = aligned_offset;
	const vk::DeviceSize suffix_size = new_block.size - prefix_size - requirements.size;

	if (prefix_size) new_block.allocations.push_back({ 0, prefix_size, false });
	new_block.allocations.push_back({ aligned_offset, requirements.size, true });
	sub_allocation* owner_ptr = &new_block.allocations.back();
	if (suffix_size) new_block.allocations.push_back({ aligned_offset + requirements.size, suffix_size, false });

	return {
		new_block.memory,
		requirements.size,
		aligned_offset,
		new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
		owner_ptr
	};
}

auto gse::vulkan::persistent_allocator::create_buffer(const config::device_config& config, const vk::BufferCreateInfo& buffer_info, const vk::MemoryPropertyFlags properties, const void* data) -> buffer_resource {
	auto buffer = config.device.createBuffer(buffer_info);
	const vk::BufferMemoryRequirementsInfo2 buffer_requirements_info{
		.buffer = buffer
	};
	const auto requirements = config.device.getBufferMemoryRequirements2(buffer_requirements_info).memoryRequirements;

	auto alloc = allocate(config, requirements, properties);
	buffer.bindMemory(alloc.memory(), alloc.offset());

	if (data && alloc.mapped()) {
		std::memcpy(alloc.mapped(), data, buffer_info.size);
	}

	return { std::move(buffer), std::move(alloc) };
}

auto gse::vulkan::persistent_allocator::create_image(const config::device_config& config, const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, vk::ImageViewCreateInfo view_info, const void* data) -> image_resource {
	vk::raii::Image image = config.device.createImage(info);
	const auto requirement_info = vk::ImageMemoryRequirementsInfo2{ .image = image };
	const auto requirements = config.device.getImageMemoryRequirements2(requirement_info).memoryRequirements;

	allocation alloc = allocate(config, requirements, properties);
	image.bindMemory(alloc.memory(), alloc.offset());

	if (data && alloc.mapped()) {
		std::memcpy(alloc.mapped(), data, requirements.size);
	}

	vk::raii::ImageView view = nullptr;

	if (view_info != vk::ImageViewCreateInfo{}) {
		assert(view_info.image == nullptr, std::format("Image view info must not have an image set yet!"));
		vk::ImageViewCreateInfo actual_view_info = view_info;
		actual_view_info.image = image;
		view = config.device.createImageView(actual_view_info);
	}
	else {
		view = config.device.createImageView({
			.flags = {},
			.image = *image,
			.viewType = vk::ImageViewType::e2D,
			.format = info.format,
			.components = {},
			.subresourceRange = {
				.aspectMask = info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment
								  ? vk::ImageAspectFlagBits::eDepth
								  : vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = info.mipLevels,
				.baseArrayLayer = 0,
				.layerCount = info.arrayLayers
			}
		});
	}

	return { std::move(image), std::move(view), std::move(alloc) };
}

auto gse::vulkan::persistent_allocator::free(allocation& alloc) -> void {
	if (!alloc.owner()) return;

	auto* sub_to_free = alloc.owner();
	sub_to_free->in_use = false;
	alloc.m_owner = nullptr;

	for (auto& [memory_type_index, blocks] : g_persistent_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
				if (&*it == sub_to_free) {
					if (const auto next_it = std::next(it); next_it != block.allocations.end() && !next_it->in_use) {
						it->size += next_it->size;
						block.allocations.erase(next_it);
					}

					if (it != block.allocations.begin()) {
						if (const auto prev_it = std::prev(it); !prev_it->in_use) {
							prev_it->size += it->size;
							block.allocations.erase(it);
						}
					}

					return;
				}
			}
		}
	}
}

auto gse::vulkan::persistent_allocator::clean_up(const vk::Device device) -> void {
	for (auto& [memory_type_index, blocks] : g_persistent_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			if (block.mapped) {
				block.mapped = nullptr;
			}
		}
		blocks.clear();
	}
	g_persistent_memory_pools.clear();
}

auto gse::vulkan::persistent_allocator::get_memory_flag_preferences(const vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags> {
	using mpf = vk::MemoryPropertyFlagBits;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer) {
		return {
			mpf::eDeviceLocal | mpf::eHostVisible,   
			mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent   
		};
	}

	if (usage & vk::BufferUsageFlagBits::eUniformBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eHostVisible
		};
	}

	return { mpf::eHostVisible | mpf::eHostCoherent };
}
