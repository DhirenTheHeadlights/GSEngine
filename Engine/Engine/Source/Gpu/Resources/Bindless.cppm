export module gse.gpu:bindless;

import std;

import :aliases;
import :shader_codegen;

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
			bindless_heaps& bindless_heaps,
			std::uint32_t capacity = shaders::bindless_texture_capacity
		);

		~bindless_texture_set();

		auto allocate(
			const image& img,
			const sampler_desc& desc
		) -> bindless_texture_slot;

		auto release(
			bindless_texture_slot slot
		) -> void;

		auto begin_frame(
			std::uint32_t frame_index
		) -> void;

	private:
		bindless_heaps* m_bindless_heaps = nullptr;
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

gse::gpu::bindless_texture_set::bindless_texture_set(bindless_heaps& bindless_heaps, const std::uint32_t capacity)
	: m_bindless_heaps(&bindless_heaps), m_capacity(capacity) {
	m_free_list.reserve(capacity);
	for (std::uint32_t i = 0; i < capacity; ++i) {
		m_free_list.push_back(capacity - 1 - i);
	}

	log::println(log::category::vulkan, "Bindless texture set created: capacity {}", capacity);
}

gse::gpu::bindless_texture_set::~bindless_texture_set() = default;

auto gse::gpu::bindless_texture_set::allocate(const image& img, const sampler_desc& desc) -> bindless_texture_slot {
	std::lock_guard lock(m_mutex);

	assert(!m_free_list.empty(), "Bindless texture set exhausted (capacity {})", m_capacity);

	const auto slot = m_free_list.back();
	m_free_list.pop_back();

	m_bindless_heaps->resource_heap().write_texture(
		{
			.index = slot
		},
		img
	);
	m_bindless_heaps->sampler_heap().write_texture_sampler(slot, desc);

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
		.retire_after = m_frame_counter + max_frames_in_flight,
	});
}

auto gse::gpu::bindless_texture_set::begin_frame(const std::uint32_t) -> void {
	std::lock_guard lock(m_mutex);
	++m_frame_counter;

	std::erase_if(
		m_pending_releases,
		[this](const pending_release& p) {
			if (m_frame_counter >= p.retire_after) {
				m_free_list.push_back(p.slot);
				return true;
			}
			return false;
		}
	);
}
