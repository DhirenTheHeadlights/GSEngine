export module gse.gpu:descriptor_heap;

import std;
import vulkan;

import :vulkan_buffer;
import :vulkan_commands;
import :vulkan_descriptor_set_layout;
import :vulkan_device;
import :vulkan_image;
import :vulkan_sampler;
import :types;

import gse.assert;
import gse.log;
import gse.core;

export namespace gse::gpu {
	struct descriptor_image_info {
		gpu::handle<vulkan::sampler> sampler;
		gpu::handle<vulkan::image_view> image_view;
		image_layout layout = image_layout::undefined;
	};

	struct descriptor_get_info {
		descriptor_type type = descriptor_type::storage_buffer;
		descriptor_address_info buffer;
		descriptor_image_info image;
		device_address acceleration_structure = 0;
	};

	class descriptor_heap;

	struct descriptor_region {
		gpu::device_size offset = 0;
		gpu::device_size size = 0;
		descriptor_heap* heap = nullptr;

		operator bool() const {
			return heap != nullptr;
		}
	};

	class descriptor_heap : non_copyable, non_movable {
	public:
		descriptor_heap(
			const vulkan::device& dev,
			const descriptor_buffer_properties& props,
			gpu::device_size capacity = 4 * 1024 * 1024
		);

		~descriptor_heap() override;

		auto allocate(
			gpu::device_size size
		) -> descriptor_region;

		auto begin_frame(
			std::uint32_t frame_index
		) -> void;

		auto allocate_transient(
			std::uint32_t frame_index,
			gpu::device_size size
		) -> descriptor_region;

		auto write_bytes(
			const descriptor_region& region,
			gpu::device_size offset_within_region,
			const void* data,
			gpu::device_size data_size
		) const -> void;

		auto descriptor(
			const descriptor_get_info& info,
			gpu::device_size descriptor_size,
			void* out
		) const -> void;

		auto write_descriptor(
			const descriptor_region& region,
			gpu::device_size binding_offset,
			const descriptor_get_info& info,
			gpu::device_size descriptor_size
		) const -> void;

		auto bind(
			gpu::handle<vulkan::command_buffer> cmd,
			bind_point point,
			gpu::handle<vulkan::pipeline_layout> layout,
			std::uint32_t first_set,
			const descriptor_region& region
		) const -> void;

		auto bind_buffer(
			gpu::handle<vulkan::command_buffer> cmd
		) const -> void;

		auto set_offset(
			gpu::handle<vulkan::command_buffer> cmd,
			bind_point point,
			gpu::handle<vulkan::pipeline_layout> layout,
			std::uint32_t first_set,
			const descriptor_region& region
		) const -> void;

		[[nodiscard]] auto layout_size(
			gpu::handle<vulkan::descriptor_set_layout> layout
		) const -> gpu::device_size;

		[[nodiscard]] auto binding_offset(
			gpu::handle<vulkan::descriptor_set_layout> layout,
			std::uint32_t binding
		) const -> gpu::device_size;

		[[nodiscard]] auto buffer_address(
			gpu::handle<vulkan::buffer> buffer
		) const -> device_address;

		[[nodiscard]] auto props(
		) const -> const descriptor_buffer_properties&;

		[[nodiscard]] auto address(
		) const -> device_address;
	private:
		auto align_up(
			gpu::device_size value
		) const -> gpu::device_size;

		auto init_transient_zone(
		) -> void;

		vk::Device m_device;
		vk::Buffer m_buffer = nullptr;
		vk::DeviceMemory m_memory = nullptr;
		device_address m_address = 0;
		void* m_mapped = nullptr;
		gpu::device_size m_capacity = 0;
		gpu::device_size m_bump_offset = 0;
		descriptor_buffer_properties m_props;
		std::mutex m_alloc_mutex;

		bool m_transient_initialized = false;
		gpu::device_size m_persistent_end = 0;
		gpu::device_size m_transient_slice_size = 0;
		std::array<std::atomic<gpu::device_size>, 2> m_transient_offsets = {};
		std::array<gpu::device_size, 2> m_transient_bases = {};
	};

	struct descriptor_binding_info {
		gpu::device_size offset = 0;
		gpu::device_size descriptor_size = 0;
		descriptor_type type = {};
	};

	class descriptor_set_writer {
	public:
		descriptor_set_writer() = default;

		descriptor_set_writer(
			descriptor_heap& heap,
			gpu::handle<vulkan::descriptor_set_layout> layout,
			gpu::device_size layout_size,
			std::vector<descriptor_binding_info> bindings
		);

		auto begin(
			std::uint32_t frame_index
		) -> descriptor_region;

		auto buffer(
			std::uint32_t binding,
			gpu::handle<vulkan::buffer> buf,
			gpu::device_size offset,
			gpu::device_size range
		) -> descriptor_set_writer&;

		auto image(
			std::uint32_t binding,
			gpu::handle<vulkan::image_view> view,
			gpu::handle<vulkan::sampler> sampler,
			image_layout layout = image_layout::general
		) -> descriptor_set_writer&;

		auto storage_image(
			std::uint32_t binding,
			gpu::handle<vulkan::image_view> view,
			image_layout layout = image_layout::general
		) -> descriptor_set_writer&;

		auto commit(
			gpu::handle<vulkan::command_buffer> cmd,
			bind_point point,
			gpu::handle<vulkan::pipeline_layout> layout,
			std::uint32_t set_index
		) const -> void;

	private:
		descriptor_heap* m_heap = nullptr;
		gpu::device_size m_layout_size = 0;
		std::vector<descriptor_binding_info> m_bindings;
		descriptor_region m_current_region;
	};
}

namespace gse::gpu {
	auto build_vk_get_info(
		const descriptor_get_info& info,
		vk::DescriptorAddressInfoEXT& addr,
		vk::DescriptorImageInfo& img
	) -> vk::DescriptorGetInfoEXT;
}

auto gse::gpu::build_vk_get_info(const descriptor_get_info& info, vk::DescriptorAddressInfoEXT& addr, vk::DescriptorImageInfo& img) -> vk::DescriptorGetInfoEXT {
	addr = vk::DescriptorAddressInfoEXT{
		.address = info.buffer.address,
		.range = info.buffer.range,
		.format = vk::Format::eUndefined,
	};
	img = vk::DescriptorImageInfo{
		.sampler = std::bit_cast<vk::Sampler>(info.image.sampler),
		.imageView = std::bit_cast<vk::ImageView>(info.image.image_view),
		.imageLayout = vulkan::to_vk(info.image.layout),
	};

	vk::DescriptorGetInfoEXT vk_info{ .type = vulkan::to_vk(info.type) };
	switch (info.type) {
		case descriptor_type::uniform_buffer:
			vk_info.data.pUniformBuffer = &addr;
			break;
		case descriptor_type::storage_buffer:
			vk_info.data.pStorageBuffer = &addr;
			break;
		case descriptor_type::combined_image_sampler:
			vk_info.data.pCombinedImageSampler = &img;
			break;
		case descriptor_type::sampled_image:
			vk_info.data.pSampledImage = &img;
			break;
		case descriptor_type::storage_image:
			vk_info.data.pStorageImage = &img;
			break;
		case descriptor_type::sampler:
			vk_info.data.pSampler = &img.sampler;
			break;
		case descriptor_type::acceleration_structure:
			vk_info.data.accelerationStructure = info.acceleration_structure;
			break;
	}
	return vk_info;
}

gse::gpu::descriptor_heap::descriptor_heap(const vulkan::device& dev, const descriptor_buffer_properties& props, gpu::device_size capacity) : m_device(*dev.raii_device()), m_capacity(capacity), m_props(props) {
	const auto& physical_device = dev.physical_device();
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

auto gse::gpu::descriptor_heap::allocate(const gpu::device_size size) -> descriptor_region {
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
		.heap = this,
	};
}

auto gse::gpu::descriptor_heap::write_bytes(const descriptor_region& region, const gpu::device_size offset_within_region, const void* data, const gpu::device_size data_size) const -> void {
	assert(
		offset_within_region + data_size <= region.size,
		std::source_location::current(),
		"Write exceeds descriptor region bounds"
	);

	auto* dst = static_cast<std::byte*>(m_mapped) + region.offset + offset_within_region;
	std::memcpy(dst, data, data_size);
}

auto gse::gpu::descriptor_heap::descriptor(const descriptor_get_info& info, const gpu::device_size descriptor_size, void* out) const -> void {
	vk::DescriptorAddressInfoEXT addr;
	vk::DescriptorImageInfo img;
	const auto vk_info = build_vk_get_info(info, addr, img);
	m_device.getDescriptorEXT(vk_info, descriptor_size, out);
}

auto gse::gpu::descriptor_heap::write_descriptor(const descriptor_region& region, const gpu::device_size binding_offset, const descriptor_get_info& info, const gpu::device_size descriptor_size) const -> void {
	std::vector<std::byte> scratch(descriptor_size);
	descriptor(info, descriptor_size, scratch.data());

	write_bytes(region, binding_offset, scratch.data(), descriptor_size);
}

auto gse::gpu::descriptor_heap::bind(const gpu::handle<vulkan::command_buffer> cmd, const bind_point point, const gpu::handle<vulkan::pipeline_layout> layout, const std::uint32_t first_set, const descriptor_region& region) const -> void {
	set_offset(cmd, point, layout, first_set, region);
}

auto gse::gpu::descriptor_heap::bind_buffer(const gpu::handle<vulkan::command_buffer> cmd) const -> void {
	const vk::DescriptorBufferBindingInfoEXT binding{
		.address = m_address,
		.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
			| vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT
	};
	std::bit_cast<vk::CommandBuffer>(cmd).bindDescriptorBuffersEXT(1, &binding);
}

auto gse::gpu::descriptor_heap::set_offset(const gpu::handle<vulkan::command_buffer> cmd, const bind_point point, const gpu::handle<vulkan::pipeline_layout> layout, const std::uint32_t first_set, const descriptor_region& region) const -> void {
	constexpr std::uint32_t buffer_index = 0;
	std::bit_cast<vk::CommandBuffer>(cmd).setDescriptorBufferOffsetsEXT(
		vulkan::to_vk(point),
		std::bit_cast<vk::PipelineLayout>(layout),
		first_set,
		1,
		&buffer_index,
		&region.offset
	);
}

auto gse::gpu::descriptor_heap::layout_size(const gpu::handle<vulkan::descriptor_set_layout> layout) const -> gpu::device_size {
	return m_device.getDescriptorSetLayoutSizeEXT(std::bit_cast<vk::DescriptorSetLayout>(layout));
}

auto gse::gpu::descriptor_heap::binding_offset(const gpu::handle<vulkan::descriptor_set_layout> layout, const std::uint32_t binding) const -> gpu::device_size {
	return m_device.getDescriptorSetLayoutBindingOffsetEXT(std::bit_cast<vk::DescriptorSetLayout>(layout), binding);
}

auto gse::gpu::descriptor_heap::buffer_address(const gpu::handle<vulkan::buffer> buffer) const -> device_address {
	return m_device.getBufferAddress({ .buffer = std::bit_cast<vk::Buffer>(buffer) });
}

auto gse::gpu::descriptor_heap::props() const -> const descriptor_buffer_properties& {
	return m_props;
}

auto gse::gpu::descriptor_heap::address() const -> device_address {
	return m_address;
}

auto gse::gpu::descriptor_heap::begin_frame(const std::uint32_t frame_index) -> void {
	if (!m_transient_initialized) {
		init_transient_zone();
	}
	m_transient_offsets[frame_index].store(m_transient_bases[frame_index], std::memory_order_relaxed);
}

auto gse::gpu::descriptor_heap::allocate_transient(std::uint32_t frame_index, const gpu::device_size size) -> descriptor_region {
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
		.heap = this,
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

auto gse::gpu::descriptor_heap::align_up(const gpu::device_size value) const -> gpu::device_size {
	const auto alignment = m_props.offset_alignment;
	return (value + alignment - 1) & ~(alignment - 1);
}

gse::gpu::descriptor_set_writer::descriptor_set_writer(descriptor_heap& heap, gpu::handle<vulkan::descriptor_set_layout> layout, const gpu::device_size layout_size, std::vector<descriptor_binding_info> bindings) : m_heap(&heap), m_layout_size(layout_size), m_bindings(std::move(bindings)) {}

auto gse::gpu::descriptor_set_writer::begin(const std::uint32_t frame_index) -> descriptor_region {
	m_current_region = m_heap->allocate_transient(frame_index, m_layout_size);
	return m_current_region;
}

auto gse::gpu::descriptor_set_writer::buffer(std::uint32_t binding, const gpu::handle<vulkan::buffer> buf, const gpu::device_size offset, const gpu::device_size range) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& [binding_offset, descriptor_size, type] = m_bindings[binding];
	const auto addr = m_heap->buffer_address(buf);

	const bool is_uniform = type == descriptor_type::uniform_buffer;

	const descriptor_get_info get_info{
		.type = is_uniform ? descriptor_type::uniform_buffer : descriptor_type::storage_buffer,
		.buffer = {
			.address = addr + offset,
			.range = range,
		},
	};

	m_heap->write_descriptor(m_current_region, binding_offset, get_info, descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::image(std::uint32_t binding, const gpu::handle<vulkan::image_view> view, const gpu::handle<vulkan::sampler> sampler, const image_layout layout) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& info = m_bindings[binding];

	const descriptor_get_info get_info{
		.type = descriptor_type::combined_image_sampler,
		.image = {
			.sampler = sampler,
			.image_view = view,
			.layout = layout,
		},
	};

	m_heap->write_descriptor(m_current_region, info.offset, get_info, info.descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::storage_image(const std::uint32_t binding, const gpu::handle<vulkan::image_view> view, const image_layout layout) -> descriptor_set_writer& {
	assert(binding < m_bindings.size(), std::source_location::current(),
		"Binding {} out of range (max {})", binding, m_bindings.size());

	const auto& info = m_bindings[binding];

	const descriptor_get_info get_info{
		.type = descriptor_type::storage_image,
		.image = {
			.sampler = {},
			.image_view = view,
			.layout = layout,
		},
	};

	m_heap->write_descriptor(m_current_region, info.offset, get_info, info.descriptor_size);
	return *this;
}

auto gse::gpu::descriptor_set_writer::commit(const gpu::handle<vulkan::command_buffer> cmd, const bind_point point, const gpu::handle<vulkan::pipeline_layout> layout, const std::uint32_t set_index) const -> void {
	assert(m_current_region, std::source_location::current(), "Cannot commit without begin()");
	m_current_region.heap->set_offset(cmd, point, layout, set_index, m_current_region);
}
