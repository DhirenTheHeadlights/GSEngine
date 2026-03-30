module;

export module gse.platform:descriptor_heap;

import std;

import :vulkan_allocator;
import :descriptor_buffer_types;

import gse.assert;

export namespace gse::vulkan {
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

		auto write_bytes(
			const descriptor_region& region,
			vk::DeviceSize offset_within_region,
			const void* data,
			vk::DeviceSize data_size
		) -> void;

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
		) -> void;

		auto bind(
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

		vk::Device m_device;
		vk::Buffer m_buffer = nullptr;
		vk::DeviceMemory m_memory = nullptr;
		vk::DeviceAddress m_address = 0;
		void* m_mapped = nullptr;
		vk::DeviceSize m_capacity = 0;
		vk::DeviceSize m_bump_offset = 0;
		descriptor_buffer_properties m_props;
		std::mutex m_alloc_mutex;
	};
}

gse::vulkan::descriptor_heap::descriptor_heap(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const descriptor_buffer_properties& props, vk::DeviceSize capacity) : m_device(*device), m_capacity(capacity), m_props(props) {
	const vk::BufferCreateInfo buffer_info{
		.size = capacity,
		.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
			| vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT
			| vk::BufferUsageFlagBits::eShaderDeviceAddress
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

	std::println("Descriptor heap created: {} KB, address 0x{:x}", capacity / 1024, m_address);
}

gse::vulkan::descriptor_heap::~descriptor_heap() {
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

auto gse::vulkan::descriptor_heap::allocate(vk::DeviceSize size) -> descriptor_region {
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

	return {
		.offset = aligned_offset,
		.size = aligned_size,
		.heap = this
	};
}

auto gse::vulkan::descriptor_heap::write_bytes(const descriptor_region& region, vk::DeviceSize offset_within_region, const void* data, vk::DeviceSize data_size) -> void {
	assert(
		offset_within_region + data_size <= region.size,
		std::source_location::current(),
		"Write exceeds descriptor region bounds"
	);

	auto* dst = static_cast<std::byte*>(m_mapped) + region.offset + offset_within_region;
	std::memcpy(dst, data, data_size);
}

auto gse::vulkan::descriptor_heap::descriptor(const vk::DescriptorGetInfoEXT& info, vk::DeviceSize descriptor_size, void* out) const -> void {
	m_device.getDescriptorEXT(info, descriptor_size, out);
}

auto gse::vulkan::descriptor_heap::write_descriptor(const descriptor_region& region, vk::DeviceSize binding_offset, const vk::DescriptorGetInfoEXT& info,vk::DeviceSize descriptor_size) -> void {
	std::vector<std::byte> scratch(descriptor_size);
	descriptor(info, descriptor_size, scratch.data());
	write_bytes(region, binding_offset, scratch.data(), descriptor_size);
}

auto gse::vulkan::descriptor_heap::bind(vk::CommandBuffer cmd, vk::PipelineBindPoint point, vk::PipelineLayout layout, std::uint32_t first_set, const descriptor_region& region) const -> void {
	const vk::DescriptorBufferBindingInfoEXT binding{
		.address = m_address,
		.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
			| vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT
	};

	cmd.bindDescriptorBuffersEXT(1, &binding);

	constexpr std::uint32_t buffer_index = 0;
	cmd.setDescriptorBufferOffsetsEXT(
		point, layout, first_set, 1, &buffer_index, &region.offset
	);
}

auto gse::vulkan::descriptor_heap::layout_size(vk::DescriptorSetLayout layout) const -> vk::DeviceSize {
	return m_device.getDescriptorSetLayoutSizeEXT(layout);
}

auto gse::vulkan::descriptor_heap::binding_offset(vk::DescriptorSetLayout layout, std::uint32_t binding) const -> vk::DeviceSize {
	return m_device.getDescriptorSetLayoutBindingOffsetEXT(layout, binding);
}

auto gse::vulkan::descriptor_heap::buffer_address(vk::Buffer buffer) const -> vk::DeviceAddress {
	return m_device.getBufferAddress({ .buffer = buffer });
}

auto gse::vulkan::descriptor_heap::props() const -> const descriptor_buffer_properties& {
	return m_props;
}

auto gse::vulkan::descriptor_heap::address() const -> vk::DeviceAddress {
	return m_address;
}

auto gse::vulkan::descriptor_heap::align_up(vk::DeviceSize value) const -> vk::DeviceSize {
	const auto alignment = m_props.offset_alignment;
	return (value + alignment - 1) & ~(alignment - 1);
}
