export module gse.gpu:descriptor_heap;

import std;
import vulkan;

import :vulkan_allocator;
import :types;

import gse.assert;
import gse.log;

export namespace gse::gpu {
	struct descriptor_buffer_properties {
		vk::DeviceSize offset_alignment = 0;
		vk::DeviceSize uniform_buffer_descriptor_size = 0;
		vk::DeviceSize storage_buffer_descriptor_size = 0;
		vk::DeviceSize sampled_image_descriptor_size = 0;
		vk::DeviceSize sampler_descriptor_size = 0;
		vk::DeviceSize combined_image_sampler_descriptor_size = 0;
		vk::DeviceSize storage_image_descriptor_size = 0;
		vk::DeviceSize input_attachment_descriptor_size = 0;
		vk::DeviceSize acceleration_structure_descriptor_size = 0;
		bool push_descriptors_supported = false;
		bool bufferless_push_descriptors = false;
	};

	class descriptor_heap;

	struct descriptor_region {
		vk::DeviceSize offset = 0;
		vk::DeviceSize size = 0;
		descriptor_heap* heap = nullptr;

		operator bool() const { return heap != nullptr; }
	};

	class descriptor_heap : non_copyable, non_movable {
	public:
		descriptor_heap(
			const vk::raii::Device& device,
			const vk::raii::PhysicalDevice& physical_device,
			const descriptor_buffer_properties& props,
			vk::DeviceSize capacity = 4 * 1024 * 1024
		);

		~descriptor_heap() override;

		auto allocate(
			vk::DeviceSize size
		) -> descriptor_region;

		auto begin_frame(
			std::uint32_t frame_index
		) -> void;

		auto allocate_transient(
			std::uint32_t frame_index,
			vk::DeviceSize size
		) -> descriptor_region;

		auto write_bytes(
			const descriptor_region& region,
			vk::DeviceSize offset_within_region,
			const void* data,
			vk::DeviceSize data_size
		) const -> void;

		auto descriptor(
			const vk::DescriptorGetInfoEXT& info,
			vk::DeviceSize descriptor_size,
			void* out
		) const -> void;

		auto write_descriptor(
			const descriptor_region& region,
			vk::DeviceSize binding_offset,
			const vk::DescriptorGetInfoEXT& info,
			vk::DeviceSize descriptor_size
		) const -> void;

		auto bind(
			vk::CommandBuffer cmd,
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			std::uint32_t first_set,
			const descriptor_region& region
		) const -> void;

		auto bind_buffer(
			vk::CommandBuffer cmd
		) const -> void;

		auto set_offset(
			vk::CommandBuffer cmd,
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			std::uint32_t first_set,
			const descriptor_region& region
		) const -> void;

		[[nodiscard]] auto layout_size(
			vk::DescriptorSetLayout layout
		) const -> vk::DeviceSize;

		[[nodiscard]] auto binding_offset(
			vk::DescriptorSetLayout layout,
			std::uint32_t binding
		) const -> vk::DeviceSize;

		[[nodiscard]] auto buffer_address(
			vk::Buffer buffer
		) const -> vk::DeviceAddress;

		[[nodiscard]] auto props(
		) const -> const descriptor_buffer_properties&;

		[[nodiscard]] auto address(
		) const -> vk::DeviceAddress;
	private:
		auto align_up(
			vk::DeviceSize value
		) const -> vk::DeviceSize;

		auto init_transient_zone(
		) -> void;

		vk::Device m_device;
		vk::Buffer m_buffer = nullptr;
		vk::DeviceMemory m_memory = nullptr;
		vk::DeviceAddress m_address = 0;
		void* m_mapped = nullptr;
		vk::DeviceSize m_capacity = 0;
		vk::DeviceSize m_bump_offset = 0;
		descriptor_buffer_properties m_props;
		std::mutex m_alloc_mutex;

		bool m_transient_initialized = false;
		vk::DeviceSize m_persistent_end = 0;
		vk::DeviceSize m_transient_slice_size = 0;
		std::array<std::atomic<vk::DeviceSize>, 2> m_transient_offsets = {};
		std::array<vk::DeviceSize, 2> m_transient_bases = {};
	};

	struct descriptor_binding_info {
		vk::DeviceSize offset = 0;
		vk::DeviceSize descriptor_size = 0;
		vk::DescriptorType type = {};
	};

	class descriptor_set_writer {
	public:
		descriptor_set_writer() = default;

		descriptor_set_writer(
			descriptor_heap& heap,
			vk::DescriptorSetLayout layout,
			vk::DeviceSize layout_size,
			std::vector<descriptor_binding_info> bindings
		);

		auto begin(
			std::uint32_t frame_index
		) -> descriptor_region;

		auto buffer(
			std::uint32_t binding,
			vk::Buffer buf,
			vk::DeviceSize offset,
			vk::DeviceSize range
		) -> descriptor_set_writer&;

		auto image(
			std::uint32_t binding,
			vk::ImageView view,
			vk::Sampler sampler,
			vk::ImageLayout layout = vk::ImageLayout::eGeneral
		) -> descriptor_set_writer&;

		auto storage_image(
			std::uint32_t binding,
			vk::ImageView view,
			vk::ImageLayout layout = vk::ImageLayout::eGeneral
		) -> descriptor_set_writer&;

		auto commit(
			vk::CommandBuffer cmd,
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			std::uint32_t set_index
		) const -> void;

	private:
		descriptor_heap* m_heap = nullptr;
		vk::DeviceSize m_layout_size = 0;
		std::vector<descriptor_binding_info> m_bindings;
		descriptor_region m_current_region;
	};
}

gse::gpu::descriptor_heap::descriptor_heap(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const descriptor_buffer_properties& props, vk::DeviceSize capacity) : m_device(*device), m_capacity(capacity), m_props(props) {
	vk::BufferUsageFlags descriptor_buffer_usage =
		vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
		| vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT;

	const vk::BufferCreateInfo buffer_info{
		.size = capacity,
		.usage = descriptor_buffer_usage | vk::BufferUsageFlagBits::eShaderDeviceAddress
	};

	m_buffer = m_device.createBuffer(buffer_info, nullptr);

	const auto mem_reqs = m_device.getBufferMemoryRequirements(m_buffer);
	const auto mem_props = physical_device.getMemoryProperties();

	std::uint32_t memory_type_index = std::numeric_limits<std::uint32_t>::max();
	constexpr auto required_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((mem_reqs.memoryTypeBits & (1u << i))
			&& (mem_props.memoryTypes[i].propertyFlags & required_flags) == required_flags) {
			memory_type_index = i;
			break;
		}
	}

	assert(
		memory_type_index != std::numeric_limits<std::uint32_t>::max(),
		std::source_location::current(),
		"No suitable memory type for descriptor heap"
	);

	const vk::MemoryAllocateFlagsInfo flags_info{
		.flags = vk::MemoryAllocateFlagBits::eDeviceAddress
	};

	const vk::MemoryAllocateInfo alloc_info{
		.pNext = &flags_info,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = memory_type_index
	};

	m_memory = m_device.allocateMemory(alloc_info, nullptr);
	m_device.bindBufferMemory(m_buffer, m_memory, 0);

	m_mapped = m_device.mapMemory(m_memory, 0, capacity, {});

	const vk::BufferDeviceAddressInfo addr_info{ .buffer = m_buffer };
	m_address = m_device.getBufferAddress(addr_info);

	log::println(log::category::vulkan_memory, "Descriptor heap created: {} KB, address 0x{:x}", capacity / 1024, m_address);
}

gse::gpu::descriptor_heap::~descriptor_heap() {
	if (m_device) {
		if (m_mapped) {
			m_device.unmapMemory(m_memory);
		}
		if (m_buffer) {
			m_device.destroyBuffer(m_buffer, nullptr);
		}
		if (m_memory) {
			m_device.freeMemory(m_memory, nullptr);
		}
	}
}

auto gse::gpu::descriptor_heap::allocate(const vk::DeviceSize size) -> descriptor_region {
	std::lock_guard lock(m_alloc_mutex);

	const auto aligned_offset = align_up(m_bump_offset);
	const auto aligned_size = align_up(size);

	assert(
		aligned_offset + aligned_size <= m_capacity,
		std::source_location::current(),
		"Descriptor heap exhausted: requested {} bytes at offset {}, capacity {}",
		aligned_size, aligned_offset, m_capacity
	);

	m_bump_offset = aligned_offset + aligned_size;

	std::memset(static_cast<std::byte*>(m_mapped) + aligned_offset, 0, aligned_size);

	return {
		.offset = aligned_offset,
		.size = aligned_size,
		.heap = this
	};
}

auto gse::gpu::descriptor_heap::write_bytes(const descriptor_region& region, const vk::DeviceSize offset_within_region, const void* data, const vk::DeviceSize data_size) const -> void {
	assert(
		offset_within_region + data_size <= region.size,
		std::source_location::current(),
		"Write exceeds descriptor region bounds"
	);

	auto* dst = static_cast<std::byte*>(m_mapped) + region.offset + offset_within_region;
	std::memcpy(dst, data, data_size);
}

auto gse::gpu::descriptor_heap::descriptor(const vk::DescriptorGetInfoEXT& info, const vk::DeviceSize descriptor_size, void* out) const -> void {
	m_device.getDescriptorEXT(info, descriptor_size, out);
}

auto gse::gpu::descriptor_heap::write_descriptor(const descriptor_region& region, const vk::DeviceSize binding_offset, const vk::DescriptorGetInfoEXT& info, const vk::DeviceSize descriptor_size) const -> void {
	std::vector<std::byte> scratch(descriptor_size);
	descriptor(info, descriptor_size, scratch.data());

	write_bytes(region, binding_offset, scratch.data(), descriptor_size);
}

auto gse::gpu::descriptor_heap::bind(const vk::CommandBuffer cmd, const vk::PipelineBindPoint point, const vk::PipelineLayout layout, const std::uint32_t first_set, const descriptor_region& region) const -> void {
	set_offset(cmd, point, layout, first_set, region);
}

auto gse::gpu::descriptor_heap::bind_buffer(const vk::CommandBuffer cmd) const -> void {
	const vk::DescriptorBufferBindingInfoEXT binding{
		.address = m_address,
		.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
			| vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT
	};
	cmd.bindDescriptorBuffersEXT(1, &binding);
}

auto gse::gpu::descriptor_heap::set_offset(const vk::CommandBuffer cmd, const vk::PipelineBindPoint point, const vk::PipelineLayout layout, const std::uint32_t first_set, const descriptor_region& region) const -> void {
	constexpr std::uint32_t buffer_index = 0;
	cmd.setDescriptorBufferOffsetsEXT(
		point, layout, first_set, 1, &buffer_index, &region.offset
	);
}

auto gse::gpu::descriptor_heap::layout_size(const vk::DescriptorSetLayout layout) const -> vk::DeviceSize {
	return m_device.getDescriptorSetLayoutSizeEXT(layout);
}

auto gse::gpu::descriptor_heap::binding_offset(const vk::DescriptorSetLayout layout, const std::uint32_t binding) const -> vk::DeviceSize {
	return m_device.getDescriptorSetLayoutBindingOffsetEXT(layout, binding);
}

auto gse::gpu::descriptor_heap::buffer_address(const vk::Buffer buffer) const -> vk::DeviceAddress {
	return m_device.getBufferAddress({ .buffer = buffer });
}

auto gse::gpu::descriptor_heap::props() const -> const descriptor_buffer_properties& {
	return m_props;
}

auto gse::gpu::descriptor_heap::address() const -> vk::DeviceAddress {
	return m_address;
}

auto gse::gpu::descriptor_heap::begin_frame(const std::uint32_t frame_index) -> void {
	if (!m_transient_initialized) {
		init_transient_zone();
	}
	m_transient_offsets[frame_index].store(m_transient_bases[frame_index], std::memory_order_relaxed);
}

auto gse::gpu::descriptor_heap::allocate_transient(std::uint32_t frame_index, const vk::DeviceSize size) -> descriptor_region {
	assert(m_transient_initialized, std::source_location::current(), "begin_frame must be called before allocate_transient");

	const auto aligned_size = align_up(size);
	const auto offset = m_transient_offsets[frame_index].fetch_add(aligned_size, std::memory_order_relaxed);
	const auto slice_end = m_transient_bases[frame_index] + m_transient_slice_size;

	assert(
		offset + aligned_size <= slice_end,
		std::source_location::current(),
		"Transient descriptor heap slice exhausted for frame {}: requested {} at {}, slice ends at {}",
		frame_index, aligned_size, offset, slice_end
	);

	return {
		.offset = offset,
		.size = aligned_size,
		.heap = this
	};
}

auto gse::gpu::descriptor_heap::init_transient_zone() -> void {
	m_persistent_end = align_up(m_bump_offset);
	const auto remaining = m_capacity - m_persistent_end;
	constexpr std::uint32_t frames = 2;
	m_transient_slice_size = (remaining / frames) & ~(m_props.offset_alignment - 1);

	for (std::uint32_t i = 0; i < frames; ++i) {
		m_transient_bases[i] = m_persistent_end + i * m_transient_slice_size;
		m_transient_offsets[i].store(m_transient_bases[i], std::memory_order_relaxed);
	}

	m_transient_initialized = true;

	log::println(log::category::vulkan_memory,
		"Descriptor heap transient zone: persistent={} bytes, {} slices x {} KB each",
		m_persistent_end, frames, m_transient_slice_size / 1024);
}

auto gse::gpu::descriptor_heap::align_up(const vk::DeviceSize value) const -> vk::DeviceSize {
	const auto alignment = m_props.offset_alignment;
	return (value + alignment - 1) & ~(alignment - 1);
}

gse::gpu::descriptor_set_writer::descriptor_set_writer(
	descriptor_heap& heap,
	vk::DescriptorSetLayout layout,
	const vk::DeviceSize layout_size,
	std::vector<descriptor_binding_info> bindings
) : m_heap(&heap), m_layout_size(layout_size), m_bindings(std::move(bindings)) {}

auto gse::gpu::descriptor_set_writer::begin(const std::uint32_t frame_index) -> descriptor_region {
	m_current_region = m_heap->allocate_transient(frame_index, m_layout_size);
	return m_current_region;
}

auto gse::gpu::descriptor_set_writer::buffer(
	std::uint32_t binding,
	const vk::Buffer buf,
	const vk::DeviceSize offset,
	const vk::DeviceSize range
) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& [binding_offset, descriptor_size, type] = m_bindings[binding];
	const auto addr = m_heap->buffer_address(buf);

	const vk::DescriptorAddressInfoEXT addr_info{
		.address = addr + offset,
		.range = range,
		.format = vk::Format::eUndefined
	};

	const bool is_uniform = type == vk::DescriptorType::eUniformBuffer
		|| type == vk::DescriptorType::eUniformBufferDynamic;

	const vk::DescriptorGetInfoEXT get_info{
		.type = is_uniform ? vk::DescriptorType::eUniformBuffer : vk::DescriptorType::eStorageBuffer,
		.data = is_uniform
			? vk::DescriptorDataEXT{ .pUniformBuffer = &addr_info }
			: vk::DescriptorDataEXT{ .pStorageBuffer = &addr_info }
	};

	m_heap->write_descriptor(m_current_region, binding_offset, get_info, descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::image(
	std::uint32_t binding,
	const vk::ImageView view,
	const vk::Sampler sampler,
	const vk::ImageLayout layout
) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& info = m_bindings[binding];

	const vk::DescriptorImageInfo img_info{
		.sampler = sampler,
		.imageView = view,
		.imageLayout = layout
	};

	const vk::DescriptorGetInfoEXT get_info{
		.type = vk::DescriptorType::eCombinedImageSampler,
		.data = { .pCombinedImageSampler = &img_info }
	};

	m_heap->write_descriptor(m_current_region, info.offset, get_info, info.descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::storage_image(
	const std::uint32_t binding,
	const vk::ImageView view,
	const vk::ImageLayout layout
) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& info = m_bindings[binding];

	const vk::DescriptorImageInfo img_info{
		.sampler = nullptr,
		.imageView = view,
		.imageLayout = layout
	};

	const vk::DescriptorGetInfoEXT get_info{
		.type = vk::DescriptorType::eStorageImage,
		.data = { .pStorageImage = &img_info }
	};

	m_heap->write_descriptor(m_current_region, info.offset, get_info, info.descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::commit(
	const vk::CommandBuffer cmd,
	const vk::PipelineBindPoint point,
	const vk::PipelineLayout layout,
	const std::uint32_t set_index
) const -> void {
	assert(m_current_region, std::source_location::current(), "Cannot commit without begin()");
	m_current_region.heap->set_offset(cmd, point, layout, set_index, m_current_region);
}
