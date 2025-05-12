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

constexpr vk::DeviceSize g_persistent_default_block_size = 1024 * 1024 * 64; // 64 MB

std::unordered_map<std::uint32_t, pool> g_persistent_memory_pools;

auto gse::vulkan::persistent_allocator::allocate(const config::device_config config, const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties) -> allocation {
	const auto mem_properties = get_memory_properties(config.physical_device);
	auto memory_type_index = std::numeric_limits<std::uint32_t>::max();

	for (std::uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if (requirements.memoryTypeBits & 1 << i && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			memory_type_index = i;
			break;
		}
	}

	assert(memory_type_index != std::numeric_limits<std::uint32_t>::max(), std::format("Failed to find suitable memory type for allocation!"));

	auto& pool = g_persistent_memory_pools[memory_type_index];
	if (pool.blocks.empty()) {
		pool.memory_type_index = memory_type_index;
	}

	memory_block*  best_block = nullptr;
	std::size_t    best_index = std::numeric_limits<std::size_t>::max();
	vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
	vk::DeviceSize aligned_offset = 0;

	for (auto& block : pool.blocks) {
		if (block.properties != properties) continue;

		for (std::size_t i = 0; i < block.allocations.size(); ++i) {
			const auto& sub = block.allocations[i];
			if (sub.in_use) continue;

			const vk::DeviceSize aligned = sub.offset + requirements.alignment - 1 & ~(requirements.alignment - 1);
			if (aligned + requirements.size > sub.offset + sub.size) continue;
			const vk::DeviceSize leftover = sub.size - (aligned - sub.offset) - requirements.size;

			if (leftover < best_leftover) {
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
		const auto sub_copy = vec[best_index]; 

		const vk::DeviceSize prefix = aligned_offset - sub_copy.offset;
		const vk::DeviceSize suffix = sub_copy.size - prefix - requirements.size;

		vec[best_index] = { aligned_offset, requirements.size, true };

		std::size_t insert_at = best_index;

		if (suffix) {
			vec.insert(vec.begin() + ++insert_at, { aligned_offset + requirements.size, suffix, false });
		}

		if (prefix) {
			vec.insert(vec.begin() + best_index, { sub_copy.offset, prefix, false });
		}

		return {
			best_block->memory,
			requirements.size,
			aligned_offset,
			best_block->mapped ? static_cast<char*>(best_block->mapped) + aligned_offset : nullptr,
			&vec[best_index] 
		};
	}

	auto& new_block = pool.blocks.emplace_back();
	new_block.size = std::max(requirements.size, g_persistent_default_block_size);
	new_block.properties = properties;
	new_block.memory = config.device.allocateMemory({ new_block.size, memory_type_index });
	new_block.allocations.push_back({ .offset = 0, .size = new_block.size, .in_use = false });

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = config.device.mapMemory(new_block.memory, 0, new_block.size);
	}

	auto& sub = new_block.allocations.front();
	aligned_offset = sub.offset + requirements.alignment - 1 & ~(requirements.alignment - 1);
	sub.in_use = true;

	return {
		.memory = new_block.memory,
		.size = requirements.size,
		.offset = aligned_offset,
		.mapped = new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
		.owner = &sub
	};
}

auto gse::vulkan::persistent_allocator::bind(const config::device_config config, const vk::Buffer buffer, const vk::MemoryPropertyFlags properties, vk::MemoryRequirements requirements) -> allocation {
	if (requirements.size == 0) {
		requirements = config.device.getBufferMemoryRequirements(buffer);
	}

	const auto& allocation = allocate(config, requirements, properties);
	config.device.bindBufferMemory(buffer, allocation.memory, allocation.offset);
	return allocation;
}

auto gse::vulkan::persistent_allocator::bind(const config::device_config config, const vk::Image image, const vk::MemoryPropertyFlags properties, vk::MemoryRequirements requirements) -> allocation {
	if (requirements.size == 0) {
		requirements = config.device.getImageMemoryRequirements(image);
	}

	const auto& allocation = allocate(config, requirements, properties);
	config.device.bindImageMemory(image, allocation.memory, allocation.offset);
	return allocation;
}

auto gse::vulkan::persistent_allocator::create_buffer(const config::device_config config, const vk::BufferCreateInfo& buffer_info, const vk::MemoryPropertyFlags properties, const void* data) -> buffer_resource {
	const auto buffer = config.device.createBuffer(buffer_info);
	const auto requirements = config.device.getBufferMemoryRequirements(buffer);
	const auto alloc = bind(config, buffer, properties, requirements);

	if (data && alloc.mapped) {
		std::memcpy(alloc.mapped, data, requirements.size);
	}

	return { buffer, alloc };
}

auto gse::vulkan::persistent_allocator::create_image(const config::device_config config, const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, vk::ImageViewCreateInfo view_info, const void* data) -> image_resource {
	vk::Image image = config.device.createImage(info);
	const auto requirements = config.device.getImageMemoryRequirements(image);

	const allocation alloc = bind(config, image, properties, requirements);

	if (data && alloc.mapped) {
		std::memcpy(alloc.mapped, data, requirements.size);
	}

	vk::ImageView view;

	if (view_info != vk::ImageViewCreateInfo{}) {
		assert(view_info.image == nullptr, std::format("Image view info must not have an image set yet!"));
		vk::ImageViewCreateInfo actual_view_info = view_info;
		actual_view_info.image = image;
		view = config.device.createImageView(actual_view_info);
	}
	else {
		// Create default view
		view = config.device.createImageView({
			{}, image,
			vk::ImageViewType::e2D,
			info.format,
			{}, // Default swizzle
			{
				(info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
					? vk::ImageAspectFlagBits::eDepth
					: vk::ImageAspectFlagBits::eColor,
				0, info.mipLevels, 0, info.arrayLayers
			}
			});
	}

	return { image, view, alloc };
}

auto gse::vulkan::persistent_allocator::free(allocation& alloc) -> void {
	if (!alloc.owner) return;

	auto* sub = alloc.owner;
	sub->in_use = false;
	alloc.owner = nullptr;

	for (auto& [memory_type_index, blocks] : g_persistent_memory_pools | std::views::values) {
		for (auto& block : blocks) {
			if (block.allocations.empty()) continue;

			if (sub < &block.allocations.front() || sub > &block.allocations.back()) continue;

			auto& allocations = block.allocations;
			std::size_t write = 0;

			for (std::size_t read = 1; read < allocations.size(); ++read) {
				auto& [offset, size, in_use] = allocations[write];

				if (const auto& cur = allocations[read]; !in_use && !cur.in_use && offset + size == cur.offset) {
					size += cur.size; // merge
				}
				else {
					++write;
					if (write != read) allocations[write] = cur;
				}
			}

			allocations.resize(write + 1);

			return;
		}
	}
}


auto gse::vulkan::persistent_allocator::free(const config::device_config config, buffer_resource& resource) -> void {
	config.device.destroyBuffer(resource.buffer);
	free(resource.allocation);
	resource.buffer = nullptr;
	resource.allocation = {};
}

auto gse::vulkan::persistent_allocator::free(const config::device_config config, image_resource& resource) -> void {
	config.device.destroyImage(resource.image);
	config.device.destroyImageView(resource.view);
	free(resource.allocation);
	resource.image = nullptr;
	resource.view = nullptr;
	resource.allocation = {};
}
