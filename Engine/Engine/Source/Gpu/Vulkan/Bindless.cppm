export module gse.gpu:bindless;

import std;
import vulkan;

import :types;
import :descriptor_heap;

import :vulkan_descriptor_set_layout;
import :vulkan_device;
import :vulkan_image;
import :vulkan_sampler;
import :vulkan_sync;
import gse.assert;
import gse.log;
import gse.core;

export namespace gse::gpu {
	struct bindless_texture_slot {
		static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t index = invalid_index;

		[[nodiscard]] auto valid() const -> bool {
			return index != invalid_index;
		}
	};

	class bindless_texture_set final : public non_copyable, non_movable {
	public:
		bindless_texture_set(
			const vulkan::device& device,
			descriptor_heap& heap,
			std::uint32_t capacity = 4096
		);

		~bindless_texture_set() override;

		auto allocate(
			handle<vulkan::image_view> view,
			handle<vulkan::sampler> samp,
			image_layout layout = image_layout::general
		) -> bindless_texture_slot;

		auto release(
			bindless_texture_slot slot
		) -> void;

		auto begin_frame(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto layout_handle() const -> handle<vulkan::descriptor_set_layout>;

		[[nodiscard]] auto region() const -> const descriptor_region&;

	private:
		vk::raii::DescriptorSetLayout m_layout = nullptr;
		vk::raii::Sampler m_null_sampler = nullptr;
		descriptor_region m_region;
		descriptor_heap* m_heap = nullptr;
		device_size m_descriptor_size = 0;
		device_size m_binding_offset = 0;
		std::uint32_t m_capacity = 0;

		std::vector<std::uint32_t> m_free_list;

		struct pending_release {
			std::uint32_t slot = 0;
			std::uint64_t retire_after = 0;
		};
		std::vector<pending_release> m_pending_releases;
		std::uint64_t m_frame_counter = 0;

		std::mutex m_mutex;
	};
}

gse::gpu::bindless_texture_set::bindless_texture_set(const vulkan::device& device_cfg, descriptor_heap& heap, const std::uint32_t capacity)
	: m_heap(&heap), m_capacity(capacity) {
	const auto& device = device_cfg.raii_device();
	const vk::DescriptorSetLayoutBinding binding{
		.binding = 0,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = capacity,
		.stageFlags = vk::ShaderStageFlagBits::eAll
	};

	constexpr vk::DescriptorBindingFlags binding_flags = vk::DescriptorBindingFlagBits::ePartiallyBound;

	const vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info{
		.bindingCount = 1,
		.pBindingFlags = &binding_flags
	};

	const vk::DescriptorSetLayoutCreateInfo layout_info{
		.pNext = &flags_info,
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
		.bindingCount = 1,
		.pBindings = &binding
	};

	m_layout = device.createDescriptorSetLayout(layout_info);

	m_null_sampler = device.createSampler({
		.magFilter = vk::Filter::eNearest,
		.minFilter = vk::Filter::eNearest,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge
	});

	const auto layout_handle = std::bit_cast<handle<vulkan::descriptor_set_layout>>(*m_layout);
	m_binding_offset = heap.binding_offset(layout_handle, 0);
	const auto layout_size = heap.layout_size(layout_handle);
	m_descriptor_size = heap.props().descriptor_size_for(descriptor_type::combined_image_sampler);
	m_region = heap.allocate(layout_size);

	const descriptor_get_info null_get{
		.type = descriptor_type::combined_image_sampler,
		.image = {
			.sampler = std::bit_cast<handle<vulkan::sampler>>(*m_null_sampler),
			.image_view = {},
			.layout = image_layout::undefined,
		},
	};

	for (std::uint32_t i = 0; i < capacity; ++i) {
		m_heap->write_descriptor(
			m_region,
			m_binding_offset + i * m_descriptor_size,
			null_get,
			m_descriptor_size
		);
	}

	m_free_list.reserve(capacity);
	for (std::uint32_t i = 0; i < capacity; ++i) {
		m_free_list.push_back(capacity - 1 - i);
	}

	log::println(
		log::category::vulkan,
		"Bindless texture set created: capacity {}, layout size {} bytes, binding offset {}, descriptor size {}",
		capacity,
		layout_size,
		m_binding_offset,
		m_descriptor_size
	);
}

gse::gpu::bindless_texture_set::~bindless_texture_set() = default;

auto gse::gpu::bindless_texture_set::allocate(const handle<vulkan::image_view> view, const handle<vulkan::sampler> samp, const image_layout layout) -> bindless_texture_slot {
	std::lock_guard lock(m_mutex);

	assert(!m_free_list.empty(), "Bindless texture set exhausted (capacity {})", m_capacity);

	const auto slot = m_free_list.back();
	m_free_list.pop_back();

	const descriptor_get_info get_info{
		.type = descriptor_type::combined_image_sampler,
		.image = {
			.sampler = samp,
			.image_view = view,
			.layout = layout,
		},
	};

	m_heap->write_descriptor(
		m_region,
		m_binding_offset + slot * m_descriptor_size,
		get_info,
		m_descriptor_size
	);

	return {
		.index = slot
	};
}

auto gse::gpu::bindless_texture_set::release(const bindless_texture_slot slot) -> void {
	if (!slot.valid()) {
		return;
	}
	std::lock_guard lock(m_mutex);
	m_pending_releases.push_back({
		.slot = slot.index,
		.retire_after = m_frame_counter + vulkan::max_frames_in_flight,
	});
}

auto gse::gpu::bindless_texture_set::begin_frame(const std::uint32_t) -> void {
	std::lock_guard lock(m_mutex);
	++m_frame_counter;

	std::erase_if(m_pending_releases, [this](const pending_release& p) {
		if (m_frame_counter >= p.retire_after) {
			m_free_list.push_back(p.slot);
			return true;
		}
		return false;
	});
}

auto gse::gpu::bindless_texture_set::layout_handle() const -> handle<vulkan::descriptor_set_layout> {
	return std::bit_cast<handle<vulkan::descriptor_set_layout>>(*m_layout);
}

auto gse::gpu::bindless_texture_set::region() const -> const descriptor_region& {
	return m_region;
}
