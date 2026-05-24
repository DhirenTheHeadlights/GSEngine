export module gse.gpu:bindless_heap;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_buffer;
import :vulkan_device;
import :vulkan_image;
import :vulkan_sampler;
import :vulkan_commands;
import :vulkan_sync;

import gse.assert;
import gse.core;
import gse.log;

export namespace gse::vulkan {
	struct descriptor_heap_properties {
		gpu::device_size sampler_heap_alignment = 0;
		gpu::device_size resource_heap_alignment = 0;
		gpu::device_size max_sampler_heap_size = 0;
		gpu::device_size max_resource_heap_size = 0;
		gpu::device_size min_sampler_heap_reserved_range = 0;
		gpu::device_size min_resource_heap_reserved_range = 0;
		gpu::device_size sampler_descriptor_size = 0;
		gpu::device_size image_descriptor_size = 0;
		gpu::device_size buffer_descriptor_size = 0;
		gpu::device_size sampler_descriptor_alignment = 0;
		gpu::device_size image_descriptor_alignment = 0;
		gpu::device_size buffer_descriptor_alignment = 0;
		gpu::device_size max_push_data_size = 0;
		std::uint32_t max_embedded_samplers = 0;
		bool sparse_descriptor_heaps = false;
	};

	[[nodiscard]]
	auto query_descriptor_heap_properties(
		const vk::raii::PhysicalDevice& physical_device
	) -> descriptor_heap_properties;

	enum class bindless_resource_kind : std::uint8_t {
		sampled_image,
		storage_image,
		storage_buffer,
		uniform_buffer,
		acceleration_structure,
	};

	struct bindless_slot {
		static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t index = invalid_index;

		[[nodiscard]] auto valid() const -> bool {
			return index != invalid_index;
		}
	};

	class bindless_resource_heap final : public non_copyable, non_movable {
	public:
		bindless_resource_heap(
			const device& dev,
			const descriptor_heap_properties& props,
			std::uint32_t image_capacity = 65536,
			std::uint32_t buffer_capacity = 16384
		);

		~bindless_resource_heap() override;

		[[nodiscard]] auto allocate_image() -> bindless_slot;

		[[nodiscard]] auto allocate_buffer() -> bindless_slot;

		auto write_sampled_image(
			bindless_slot slot,
			const image& img,
			gpu::image_layout layout = gpu::image_layout::general
		) -> void;

		auto write_storage_image(
			bindless_slot slot,
			const image& img
		) -> void;

		auto write_storage_buffer(
			bindless_slot slot,
			gpu::device_address address,
			gpu::device_size size
		) -> void;

		auto write_uniform_buffer(
			bindless_slot slot,
			gpu::device_address address,
			gpu::device_size size
		) -> void;

		auto release_image(
			bindless_slot slot
		) -> void;

		auto release_buffer(
			bindless_slot slot
		) -> void;

		auto begin_frame() -> void;

		[[nodiscard]] auto buffer_address() const -> gpu::device_address;

		[[nodiscard]] auto buffer_size() const -> gpu::device_size;

		[[nodiscard]] auto reserved_offset() const -> gpu::device_size;

		[[nodiscard]] auto reserved_size() const -> gpu::device_size;

		[[nodiscard]] auto image_range_offset() const -> gpu::device_size;

		[[nodiscard]] auto image_stride() const -> gpu::device_size;

		[[nodiscard]] auto buffer_range_offset() const -> gpu::device_size;

		[[nodiscard]] auto buffer_stride() const -> gpu::device_size;

	private:
		auto write_sampled_image_vk(
			bindless_slot slot,
			const vk::ImageViewCreateInfo& view_info,
			gpu::image_layout layout
		) -> void;

		auto write_storage_image_vk(
			bindless_slot slot,
			const vk::ImageViewCreateInfo& view_info
		) -> void;

		auto write_resource(
			gpu::device_size byte_offset,
			const vk::ResourceDescriptorInfoEXT& info,
			gpu::device_size descriptor_size
		) -> void;

		struct pending_release {
			std::uint32_t slot = 0;
			std::uint64_t retire_after = 0;
		};

		vk::Device m_device;
		vk::DeviceMemory m_memory = nullptr;
		vk::Buffer m_buffer = nullptr;
		gpu::device_address m_address = 0;
		std::byte* m_mapped = nullptr;
		gpu::device_size m_size = 0;
		gpu::device_size m_reserved_size = 0;
		gpu::device_size m_image_range_offset = 0;
		gpu::device_size m_image_stride = 0;
		std::uint32_t m_image_capacity = 0;
		gpu::device_size m_buffer_range_offset = 0;
		gpu::device_size m_buffer_stride = 0;
		std::uint32_t m_buffer_capacity = 0;
		std::vector<std::uint32_t> m_image_free_list;
		std::vector<std::uint32_t> m_buffer_free_list;
		std::vector<pending_release> m_image_pending;
		std::vector<pending_release> m_buffer_pending;
		std::uint64_t m_frame_counter = 0;
		std::mutex m_mutex;
	};

	class bindless_sampler_heap final : public non_copyable, non_movable {
	public:
		bindless_sampler_heap(
			const device& dev,
			const descriptor_heap_properties& props,
			std::uint32_t capacity = 512
		);

		~bindless_sampler_heap() override;

		[[nodiscard]] auto allocate(
			const gpu::sampler_desc& desc
		) -> bindless_slot;

		auto release(
			bindless_slot slot
		) -> void;

		auto begin_frame() -> void;

		[[nodiscard]] auto buffer_address() const -> gpu::device_address;

		[[nodiscard]] auto buffer_size() const -> gpu::device_size;

		[[nodiscard]] auto reserved_offset() const -> gpu::device_size;

		[[nodiscard]] auto reserved_size() const -> gpu::device_size;

		[[nodiscard]] auto sampler_stride() const -> gpu::device_size;

	private:
		auto allocate_vk(
			const vk::SamplerCreateInfo& sampler_info
		) -> bindless_slot;

		struct pending_release {
			std::uint32_t slot = 0;
			std::uint64_t retire_after = 0;
		};

		vk::Device m_device;
		vk::DeviceMemory m_memory = nullptr;
		vk::Buffer m_buffer = nullptr;
		gpu::device_address m_address = 0;
		std::byte* m_mapped = nullptr;
		gpu::device_size m_size = 0;
		gpu::device_size m_reserved_size = 0;
		gpu::device_size m_descriptor_size = 0;
		std::uint32_t m_capacity = 0;
		std::vector<std::uint32_t> m_free_list;
		std::vector<pending_release> m_pending;
		std::uint64_t m_frame_counter = 0;
		std::mutex m_mutex;
	};

	class bindless_heaps final : public non_copyable, non_movable {
	public:
		bindless_heaps(
			const device& dev,
			std::uint32_t image_capacity = 65536,
			std::uint32_t buffer_capacity = 16384,
			std::uint32_t sampler_capacity = 512
		);

		~bindless_heaps() override = default;

		auto bind(
			const commands& cmd
		) const -> void;

		auto begin_frame() -> void;

		[[nodiscard]] auto resource_heap() -> bindless_resource_heap&;

		[[nodiscard]] auto sampler_heap() -> bindless_sampler_heap&;

		[[nodiscard]] auto resource_heap() const -> const bindless_resource_heap&;

		[[nodiscard]] auto sampler_heap() const -> const bindless_sampler_heap&;

		[[nodiscard]] auto properties() const -> const descriptor_heap_properties&;

	private:
		descriptor_heap_properties m_props;
		std::unique_ptr<bindless_resource_heap> m_resource_heap;
		std::unique_ptr<bindless_sampler_heap> m_sampler_heap;
	};

	struct bindless_mapping_result {
		std::vector<vk::DescriptorSetAndBindingMappingEXT> mappings;
		std::uint32_t push_data_size = 0;
	};

	[[nodiscard]] auto build_bindless_mappings(
		std::span<const gpu::binding_use> bindings,
		const bindless_heaps& heaps
	) -> bindless_mapping_result;
}

namespace gse::vulkan {
	struct bindless_heap_memory {
		vk::Buffer buffer = nullptr;
		vk::DeviceMemory memory = nullptr;
		gpu::device_address address = 0;
		std::byte* mapped = nullptr;
	};

	auto allocate_heap_memory(
		const device& dev,
		gpu::device_size size,
		vk::BufferUsageFlags usage,
		std::string_view tag
	) -> bindless_heap_memory;

	auto free_heap_memory(
		vk::Device dev,
		bindless_heap_memory& mem
	) -> void;

	auto align_up_to(
		gpu::device_size value,
		gpu::device_size alignment
	) -> gpu::device_size;
}

auto gse::vulkan::query_descriptor_heap_properties(const vk::raii::PhysicalDevice& physical_device) -> descriptor_heap_properties {
	const auto chain = physical_device
		.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
	const auto& dh = chain.get<vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();

	descriptor_heap_properties out{
		.sampler_heap_alignment = dh.samplerHeapAlignment,
		.resource_heap_alignment = dh.resourceHeapAlignment,
		.max_sampler_heap_size = dh.maxSamplerHeapSize,
		.max_resource_heap_size = dh.maxResourceHeapSize,
		.min_sampler_heap_reserved_range = dh.minSamplerHeapReservedRange,
		.min_resource_heap_reserved_range = dh.minResourceHeapReservedRange,
		.sampler_descriptor_size = dh.samplerDescriptorSize,
		.image_descriptor_size = dh.imageDescriptorSize,
		.buffer_descriptor_size = dh.bufferDescriptorSize,
		.sampler_descriptor_alignment = dh.samplerDescriptorAlignment,
		.image_descriptor_alignment = dh.imageDescriptorAlignment,
		.buffer_descriptor_alignment = dh.bufferDescriptorAlignment,
		.max_push_data_size = dh.maxPushDataSize,
		.max_embedded_samplers = dh.maxDescriptorHeapEmbeddedSamplers,
		.sparse_descriptor_heaps = static_cast<bool>(dh.sparseDescriptorHeaps),
	};

	log::println(log::category::vulkan, "Descriptor heap properties:");
	log::println(
		log::category::vulkan,
		"  sampler/resource alignment: {} / {}",
		out.sampler_heap_alignment,
		out.resource_heap_alignment
	);
	log::println(
		log::category::vulkan,
		"  max sampler/resource heap size: {} KB / {} MB",
		out.max_sampler_heap_size / 1024,
		out.max_resource_heap_size / (1024 * 1024)
	);
	log::println(
		log::category::vulkan,
		"  descriptor sizes: sampler={}, image={}, buffer={}",
		out.sampler_descriptor_size,
		out.image_descriptor_size,
		out.buffer_descriptor_size
	);
	log::println(
		log::category::vulkan,
		"  reserved ranges (min): sampler={} B, resource={} B",
		out.min_sampler_heap_reserved_range,
		out.min_resource_heap_reserved_range
	);
	log::println(log::category::vulkan, "  max push data size: {} B", out.max_push_data_size);

	return out;
}

auto gse::vulkan::align_up_to(const gpu::device_size value, const gpu::device_size alignment) -> gpu::device_size {
	if (alignment == 0) {
		return value;
	}
	return (value + alignment - 1) / alignment * alignment;
}

auto gse::vulkan::allocate_heap_memory(const device& dev, const gpu::device_size size, const vk::BufferUsageFlags usage, const std::string_view tag) -> bindless_heap_memory {
	const vk::PhysicalDevice physical_device = *dev.physical_device();
	const vk::Device vk_device = *dev.raii_device();

	const vk::BufferCreateInfo buffer_info{
		.size = size,
		.usage = usage | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};

	bindless_heap_memory mem;
	mem.buffer = vk_device.createBuffer(buffer_info, nullptr);

	const auto reqs = vk_device.getBufferMemoryRequirements(mem.buffer);
	const auto mem_props = physical_device.getMemoryProperties();

	std::uint32_t memory_type_index = std::numeric_limits<std::uint32_t>::max();
	constexpr auto required = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	constexpr auto preferred = required | vk::MemoryPropertyFlagBits::eDeviceLocal;

	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & preferred) == preferred) {
			memory_type_index = i;
			break;
		}
	}
	if (memory_type_index == std::numeric_limits<std::uint32_t>::max()) {
		for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
			if ((reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & required) == required) {
				memory_type_index = i;
				break;
			}
		}
	}
	assert(
		memory_type_index != std::numeric_limits<std::uint32_t>::max(),
		"No suitable memory type for bindless heap '{}'",
		tag
	);

	const vk::MemoryAllocateFlagsInfo flags_info{
		.flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
	};

	const vk::MemoryAllocateInfo alloc_info{
		.pNext = &flags_info,
		.allocationSize = reqs.size,
		.memoryTypeIndex = memory_type_index,
	};

	mem.memory = vk_device.allocateMemory(alloc_info, nullptr);
	vk_device.bindBufferMemory(mem.buffer, mem.memory, 0);
	mem.mapped = static_cast<std::byte*>(vk_device.mapMemory(mem.memory, 0, size, {}));
	mem.address = vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{
		.buffer = mem.buffer
	});

	log::println(
		log::category::vulkan_memory,
		"Bindless heap '{}' allocated: {} KB, address 0x{:x}",
		tag,
		size / 1024,
		mem.address
	);

	return mem;
}

auto gse::vulkan::free_heap_memory(const vk::Device dev, bindless_heap_memory& mem) -> void {
	if (mem.mapped && mem.memory) {
		dev.unmapMemory(mem.memory);
		mem.mapped = nullptr;
	}
	if (mem.buffer) {
		dev.destroyBuffer(mem.buffer, nullptr);
		mem.buffer = nullptr;
	}
	if (mem.memory) {
		dev.freeMemory(mem.memory, nullptr);
		mem.memory = nullptr;
	}
}

gse::vulkan::bindless_resource_heap::bindless_resource_heap(const device& dev, const descriptor_heap_properties& props, const std::uint32_t image_capacity, const std::uint32_t buffer_capacity)
	: m_device(*dev.raii_device()), m_image_stride(props.image_descriptor_size), m_image_capacity(image_capacity), m_buffer_stride(props.buffer_descriptor_size), m_buffer_capacity(buffer_capacity) {
	m_reserved_size = align_up_to(props.min_resource_heap_reserved_range, props.resource_heap_alignment);
	m_image_range_offset = m_reserved_size;
	m_buffer_range_offset = align_up_to(
		m_image_range_offset + image_capacity * m_image_stride,
		props.buffer_descriptor_alignment
	);
	m_size = m_buffer_range_offset + buffer_capacity * m_buffer_stride;
	m_size = align_up_to(m_size, props.resource_heap_alignment);

	assert(
		m_size <= props.max_resource_heap_size,
		"bindless_resource_heap: requested capacity {} bytes exceeds driver max {} bytes",
		m_size,
		props.max_resource_heap_size
	);

	auto mem = allocate_heap_memory(
		dev,
		m_size,
		vk::BufferUsageFlagBits::eDescriptorHeapEXT,
		"bindless_resource_heap"
	);
	m_buffer = mem.buffer;
	m_memory = mem.memory;
	m_address = mem.address;
	m_mapped = mem.mapped;

	m_image_free_list.reserve(image_capacity);
	for (std::uint32_t i = 0; i < image_capacity; ++i) {
		m_image_free_list.push_back(image_capacity - 1 - i);
	}
	m_buffer_free_list.reserve(buffer_capacity);
	for (std::uint32_t i = 0; i < buffer_capacity; ++i) {
		m_buffer_free_list.push_back(buffer_capacity - 1 - i);
	}

	log::println(
		log::category::vulkan,
		"bindless_resource_heap: {} KB, image slots {} @ stride {}, buffer slots {} @ stride {}, reserved {} B",
		m_size / 1024,
		image_capacity,
		m_image_stride,
		buffer_capacity,
		m_buffer_stride,
		m_reserved_size
	);
}

gse::vulkan::bindless_resource_heap::~bindless_resource_heap() {
	bindless_heap_memory mem{
		.buffer = m_buffer,
		.memory = m_memory,
		.address = m_address,
		.mapped = m_mapped,
	};
	free_heap_memory(m_device, mem);
}

auto gse::vulkan::bindless_resource_heap::allocate_image() -> bindless_slot {
	std::lock_guard lock(m_mutex);
	assert(
		!m_image_free_list.empty(),
		"bindless_resource_heap: image slots exhausted (capacity {})",
		m_image_capacity
	);
	const auto slot = m_image_free_list.back();
	m_image_free_list.pop_back();
	log::println(
		log::category::vulkan,
		"bindless heap: allocate_image -> slot {} @ byte {:#x}",
		slot,
		m_image_range_offset + slot * m_image_stride
	);
	return {
		.index = slot
	};
}

auto gse::vulkan::bindless_resource_heap::allocate_buffer() -> bindless_slot {
	std::lock_guard lock(m_mutex);
	assert(
		!m_buffer_free_list.empty(),
		"bindless_resource_heap: buffer slots exhausted (capacity {})",
		m_buffer_capacity
	);
	const auto slot = m_buffer_free_list.back();
	m_buffer_free_list.pop_back();
	return {
		.index = slot
	};
}

auto gse::vulkan::bindless_resource_heap::write_resource(const gpu::device_size byte_offset, const vk::ResourceDescriptorInfoEXT& info, const gpu::device_size descriptor_size) -> void {
	const vk::HostAddressRangeEXT dst{
		.address = m_mapped + byte_offset,
		.size = static_cast<std::size_t>(descriptor_size),
	};
	const auto result = m_device.writeResourceDescriptorsEXT(1, &info, &dst);
	assert(
		result == vk::Result::eSuccess,
		"vkWriteResourceDescriptorsEXT failed: {}",
		static_cast<std::int32_t>(result)
	);
}

auto gse::vulkan::bindless_resource_heap::write_sampled_image_vk(const bindless_slot slot, const vk::ImageViewCreateInfo& view_info, const gpu::image_layout layout) -> void {
	assert(slot.valid(), "bindless_resource_heap::write_sampled_image_vk: invalid slot");
	const vk::ImageDescriptorInfoEXT img{
		.pView = &view_info,
		.layout = to_vk(layout),
	};
	const vk::ResourceDescriptorInfoEXT info{
		.type = vk::DescriptorType::eSampledImage,
		.data = {
			.pImage = &img
		},
	};
	const auto offset = m_image_range_offset + slot.index * m_image_stride;
	write_resource(offset, info, m_image_stride);
}

auto gse::vulkan::bindless_resource_heap::write_storage_image_vk(const bindless_slot slot, const vk::ImageViewCreateInfo& view_info) -> void {
	assert(slot.valid(), "bindless_resource_heap::write_storage_image_vk: invalid slot");
	const vk::ImageDescriptorInfoEXT img{
		.pView = &view_info,
		.layout = vk::ImageLayout::eGeneral,
	};
	const vk::ResourceDescriptorInfoEXT info{
		.type = vk::DescriptorType::eStorageImage,
		.data = {
			.pImage = &img
		},
	};
	const auto offset = m_image_range_offset + slot.index * m_image_stride;
	write_resource(offset, info, m_image_stride);
}

namespace gse::vulkan {
	auto synthesize_view_info(
		const image& img
	) -> vk::ImageViewCreateInfo;
}

auto gse::vulkan::synthesize_view_info(const image& img) -> vk::ImageViewCreateInfo {
	const auto& info = img.view_create_info();
	const auto aspects = info.aspects ? info.aspects : image_aspect_for(img.format());
	return vk::ImageViewCreateInfo{
		.image = std::bit_cast<vk::Image>(img.handle()),
		.viewType = to_vk(info.view_type),
		.format = static_cast<vk::Format>(img.format()),
		.components = {},
		.subresourceRange = {
			.aspectMask = to_vk(aspects),
			.baseMipLevel = info.base_mip_level,
			.levelCount = info.level_count,
			.baseArrayLayer = info.base_array_layer,
			.layerCount = info.layer_count,
		},
	};
}

auto gse::vulkan::bindless_resource_heap::write_sampled_image(const bindless_slot slot, const image& img, const gpu::image_layout layout) -> void {
	const auto view_info = synthesize_view_info(img);
	write_sampled_image_vk(slot, view_info, layout);
}

auto gse::vulkan::bindless_resource_heap::write_storage_image(const bindless_slot slot, const image& img) -> void {
	const auto view_info = synthesize_view_info(img);
	write_storage_image_vk(slot, view_info);
}

auto gse::vulkan::bindless_resource_heap::write_storage_buffer(const bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	assert(slot.valid(), "bindless_resource_heap::write_storage_buffer: invalid slot");
	const vk::DeviceAddressRangeEXT range{
		.address = address,
		.size = size,
	};
	const vk::ResourceDescriptorInfoEXT info{
		.type = vk::DescriptorType::eStorageBuffer,
		.data = {
			.pAddressRange = &range
		},
	};
	const auto offset = m_buffer_range_offset + slot.index * m_buffer_stride;
	write_resource(offset, info, m_buffer_stride);
}

auto gse::vulkan::bindless_resource_heap::write_uniform_buffer(const bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	assert(slot.valid(), "bindless_resource_heap::write_uniform_buffer: invalid slot");
	const vk::DeviceAddressRangeEXT range{
		.address = address,
		.size = size,
	};
	const vk::ResourceDescriptorInfoEXT info{
		.type = vk::DescriptorType::eUniformBuffer,
		.data = {
			.pAddressRange = &range
		},
	};
	const auto offset = m_buffer_range_offset + slot.index * m_buffer_stride;
	write_resource(offset, info, m_buffer_stride);
}

auto gse::vulkan::bindless_resource_heap::release_image(const bindless_slot slot) -> void {
	if (!slot.valid()) {
		return;
	}
	std::lock_guard lock(m_mutex);
	m_image_pending.push_back({
		.slot = slot.index,
		.retire_after = m_frame_counter + max_frames_in_flight,
	});
}

auto gse::vulkan::bindless_resource_heap::release_buffer(const bindless_slot slot) -> void {
	if (!slot.valid()) {
		return;
	}
	std::lock_guard lock(m_mutex);
	m_buffer_pending.push_back({
		.slot = slot.index,
		.retire_after = m_frame_counter + max_frames_in_flight,
	});
}

auto gse::vulkan::bindless_resource_heap::begin_frame() -> void {
	std::lock_guard lock(m_mutex);
	++m_frame_counter;
	std::erase_if(m_image_pending, [this](const pending_release& p) {
		if (m_frame_counter >= p.retire_after) {
			m_image_free_list.push_back(p.slot);
			return true;
		}
		return false;
	});
	std::erase_if(m_buffer_pending, [this](const pending_release& p) {
		if (m_frame_counter >= p.retire_after) {
			m_buffer_free_list.push_back(p.slot);
			return true;
		}
		return false;
	});
}

auto gse::vulkan::bindless_resource_heap::buffer_address() const -> gpu::device_address {
	return m_address;
}

auto gse::vulkan::bindless_resource_heap::buffer_size() const -> gpu::device_size {
	return m_size;
}

auto gse::vulkan::bindless_resource_heap::reserved_offset() const -> gpu::device_size {
	return 0;
}

auto gse::vulkan::bindless_resource_heap::reserved_size() const -> gpu::device_size {
	return m_reserved_size;
}

auto gse::vulkan::bindless_resource_heap::image_range_offset() const -> gpu::device_size {
	return m_image_range_offset;
}

auto gse::vulkan::bindless_resource_heap::image_stride() const -> gpu::device_size {
	return m_image_stride;
}

auto gse::vulkan::bindless_resource_heap::buffer_range_offset() const -> gpu::device_size {
	return m_buffer_range_offset;
}

auto gse::vulkan::bindless_resource_heap::buffer_stride() const -> gpu::device_size {
	return m_buffer_stride;
}

gse::vulkan::bindless_sampler_heap::bindless_sampler_heap(const device& dev, const descriptor_heap_properties& props, const std::uint32_t capacity)
	: m_device(*dev.raii_device()), m_descriptor_size(props.sampler_descriptor_size), m_capacity(capacity) {
	m_reserved_size = align_up_to(props.min_sampler_heap_reserved_range, props.sampler_heap_alignment);
	const auto data_size = capacity * m_descriptor_size;
	m_size = align_up_to(m_reserved_size + data_size, props.sampler_heap_alignment);
	assert(
		m_size <= props.max_sampler_heap_size,
		"bindless_sampler_heap: requested capacity {} bytes exceeds driver max {} bytes",
		m_size,
		props.max_sampler_heap_size
	);

	auto mem = allocate_heap_memory(
		dev,
		m_size,
		vk::BufferUsageFlagBits::eDescriptorHeapEXT,
		"bindless_sampler_heap"
	);
	m_buffer = mem.buffer;
	m_memory = mem.memory;
	m_address = mem.address;
	m_mapped = mem.mapped;

	m_free_list.reserve(capacity);
	for (std::uint32_t i = 0; i < capacity; ++i) {
		m_free_list.push_back(capacity - 1 - i);
	}

	log::println(
		log::category::vulkan,
		"bindless_sampler_heap: {} B, slots {} @ stride {}, reserved {} B",
		m_size,
		capacity,
		m_descriptor_size,
		m_reserved_size
	);
}

gse::vulkan::bindless_sampler_heap::~bindless_sampler_heap() {
	bindless_heap_memory mem{
		.buffer = m_buffer,
		.memory = m_memory,
		.address = m_address,
		.mapped = m_mapped,
	};
	free_heap_memory(m_device, mem);
}

auto gse::vulkan::bindless_sampler_heap::allocate_vk(const vk::SamplerCreateInfo& sampler_info) -> bindless_slot {
	std::lock_guard lock(m_mutex);
	assert(!m_free_list.empty(), "bindless_sampler_heap: slots exhausted (capacity {})", m_capacity);
	const auto slot = m_free_list.back();
	m_free_list.pop_back();
	log::println(log::category::vulkan, "bindless heap: allocate_sampler -> slot {} @ byte {:#x}", slot, m_reserved_size + slot * m_descriptor_size);

	const auto byte_offset = m_reserved_size + slot * m_descriptor_size;
	const vk::HostAddressRangeEXT dst{
		.address = m_mapped + byte_offset,
		.size = static_cast<std::size_t>(m_descriptor_size),
	};
	const auto result = m_device.writeSamplerDescriptorsEXT(1, &sampler_info, &dst);
	assert(
		result == vk::Result::eSuccess,
		"vkWriteSamplerDescriptorsEXT failed: {}",
		static_cast<std::int32_t>(result)
	);

	return {
		.index = slot
	};
}

auto gse::vulkan::bindless_sampler_heap::allocate(const gpu::sampler_desc& desc) -> bindless_slot {
	const vk::SamplerCreateInfo info{
		.magFilter = to_vk(desc.mag),
		.minFilter = to_vk(desc.min),
		.mipmapMode = desc.min == gpu::sampler_filter::nearest
			? vk::SamplerMipmapMode::eNearest
			: vk::SamplerMipmapMode::eLinear,
		.addressModeU = to_vk(desc.address_u),
		.addressModeV = to_vk(desc.address_v),
		.addressModeW = to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = to_vk(desc.border),
		.unnormalizedCoordinates = vk::False,
	};
	return allocate_vk(info);
}

auto gse::vulkan::bindless_sampler_heap::release(const bindless_slot slot) -> void {
	if (!slot.valid()) {
		return;
	}
	std::lock_guard lock(m_mutex);
	m_pending.push_back({
		.slot = slot.index,
		.retire_after = m_frame_counter + max_frames_in_flight,
	});
}

auto gse::vulkan::bindless_sampler_heap::begin_frame() -> void {
	std::lock_guard lock(m_mutex);
	++m_frame_counter;
	std::erase_if(m_pending, [this](const pending_release& p) {
		if (m_frame_counter >= p.retire_after) {
			m_free_list.push_back(p.slot);
			return true;
		}
		return false;
	});
}

auto gse::vulkan::bindless_sampler_heap::buffer_address() const -> gpu::device_address {
	return m_address;
}

auto gse::vulkan::bindless_sampler_heap::buffer_size() const -> gpu::device_size {
	return m_size;
}

auto gse::vulkan::bindless_sampler_heap::reserved_offset() const -> gpu::device_size {
	return 0;
}

auto gse::vulkan::bindless_sampler_heap::reserved_size() const -> gpu::device_size {
	return m_reserved_size;
}

auto gse::vulkan::bindless_sampler_heap::sampler_stride() const -> gpu::device_size {
	return m_descriptor_size;
}

gse::vulkan::bindless_heaps::bindless_heaps(const device& dev, const std::uint32_t image_capacity, const std::uint32_t buffer_capacity, const std::uint32_t sampler_capacity)
	: m_props(query_descriptor_heap_properties(dev.physical_device())), m_resource_heap(std::make_unique<bindless_resource_heap>(
																			dev,
																			m_props,
																			image_capacity,
																			buffer_capacity
																		)),
	  m_sampler_heap(std::make_unique<bindless_sampler_heap>(
		  dev,
		  m_props,
		  sampler_capacity
	  )) {
}

auto gse::vulkan::bindless_heaps::bind(const commands& cmd) const -> void {
	cmd.bind_resource_heap(
		m_resource_heap->buffer_address(),
		m_resource_heap->buffer_size(),
		m_resource_heap->reserved_offset(),
		m_resource_heap->reserved_size()
	);
	cmd.bind_sampler_heap(
		m_sampler_heap->buffer_address(),
		m_sampler_heap->buffer_size(),
		m_sampler_heap->reserved_offset(),
		m_sampler_heap->reserved_size()
	);
}

auto gse::vulkan::bindless_heaps::begin_frame() -> void {
	m_resource_heap->begin_frame();
	m_sampler_heap->begin_frame();
}

auto gse::vulkan::bindless_heaps::resource_heap() -> bindless_resource_heap& {
	return *m_resource_heap;
}

auto gse::vulkan::bindless_heaps::sampler_heap() -> bindless_sampler_heap& {
	return *m_sampler_heap;
}

auto gse::vulkan::bindless_heaps::resource_heap() const -> const bindless_resource_heap& {
	return *m_resource_heap;
}

auto gse::vulkan::bindless_heaps::sampler_heap() const -> const bindless_sampler_heap& {
	return *m_sampler_heap;
}

auto gse::vulkan::bindless_heaps::properties() const -> const descriptor_heap_properties& {
	return m_props;
}

auto gse::vulkan::build_bindless_mappings(const std::span<const gpu::binding_use> bindings, const bindless_heaps& heaps) -> bindless_mapping_result {
	bindless_mapping_result result;
	result.mappings.reserve(bindings.size());

	std::vector<gpu::binding_use> sorted_bindings(bindings.begin(), bindings.end());
	std::ranges::sort(sorted_bindings, [](const gpu::binding_use& a, const gpu::binding_use& b) {
		if (a.set != b.set) {
			return a.set < b.set;
		}
		return a.slot < b.slot;
	});

	const auto& resource_heap = heaps.resource_heap();
	const auto& sampler_heap = heaps.sampler_heap();
	const auto image_offset = static_cast<std::uint32_t>(resource_heap.image_range_offset());
	const auto image_stride = static_cast<std::uint32_t>(resource_heap.image_stride());
	const auto buffer_offset = static_cast<std::uint32_t>(resource_heap.buffer_range_offset());
	const auto buffer_stride = static_cast<std::uint32_t>(resource_heap.buffer_stride());
	const auto sampler_offset = static_cast<std::uint32_t>(sampler_heap.reserved_size());
	const auto sampler_stride = static_cast<std::uint32_t>(sampler_heap.sampler_stride());

	std::uint32_t push_offset = 0;
	for (const auto& b : sorted_bindings) {
		vk::DescriptorMappingSourceDataEXT source_data{};
		auto& pi = source_data.pushIndex;

		const auto write_resource_fields = [&](const std::uint32_t heap_off, const std::uint32_t stride) {
			pi.heapOffset = heap_off;
			pi.pushOffset = push_offset;
			pi.heapIndexStride = stride;
			pi.heapArrayStride = 0;
			push_offset += static_cast<std::uint32_t>(sizeof(std::uint32_t));
		};
		const auto write_sampler_fields = [&]() {
			pi.samplerHeapOffset = sampler_offset;
			pi.samplerPushOffset = push_offset;
			pi.samplerHeapIndexStride = sampler_stride;
			pi.samplerHeapArrayStride = 0;
			push_offset += static_cast<std::uint32_t>(sizeof(std::uint32_t));
		};

		vk::SpirvResourceTypeFlagsEXT resource_mask{};
		switch (b.type) {
			case gpu::descriptor_type::sampled_image:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampledImage;
				write_resource_fields(image_offset, image_stride);
				break;
			case gpu::descriptor_type::storage_image:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyImage | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteImage;
				write_resource_fields(image_offset, image_stride);
				break;
			case gpu::descriptor_type::combined_image_sampler:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eCombinedSampledImage;
				pi.useCombinedImageSamplerIndex = vk::False;
				write_resource_fields(image_offset, image_stride);
				write_sampler_fields();
				break;
			case gpu::descriptor_type::sampler:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampler;
				write_sampler_fields();
				break;
			case gpu::descriptor_type::uniform_buffer:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eUniformBuffer;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
			case gpu::descriptor_type::storage_buffer:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyStorageBuffer | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteStorageBuffer;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
			case gpu::descriptor_type::acceleration_structure:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eAccelerationStructure;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
		}

		result.mappings.push_back(vk::DescriptorSetAndBindingMappingEXT{
			.descriptorSet = b.set,
			.firstBinding = b.slot,
			.bindingCount = 1,
			.resourceMask = resource_mask,
			.source = vk::DescriptorMappingSourceEXT::eHeapWithPushIndex,
			.sourceData = source_data,
		});
	}

	result.push_data_size = push_offset;

	log::println(
		log::category::vulkan,
		"bindless mappings: {} bindings, total push_data {} bytes",
		result.mappings.size(),
		result.push_data_size
	);
	for (std::size_t i = 0; i < sorted_bindings.size(); ++i) {
		const auto& b = sorted_bindings[i];
		const auto& m = result.mappings[i];
		const auto& pi = m.sourceData.pushIndex;
		log::println(
			log::category::vulkan,
			"  set={} binding={} type={} mask={:#x} heap_offset={:#x} push_offset={} stride={} sampler_heap_offset={:#x} sampler_push_offset={} use_combined={}",
			b.set,
			b.slot,
			static_cast<int>(b.type),
			static_cast<std::uint32_t>(m.resourceMask),
			pi.heapOffset,
			pi.pushOffset,
			pi.heapIndexStride,
			pi.samplerHeapOffset,
			pi.samplerPushOffset,
			static_cast<bool>(pi.useCombinedImageSamplerIndex)
		);
	}

	return result;
}
