export module gse.gpu_backend:bindless;

import std;

import :core;

import gse.assert;
import gse.core;

export namespace gse::gpu {
	struct descriptor_heap {};

	enum class image_descriptor_kind : std::uint8_t {
		sampled,
		storage,
		texture,
	};

	enum class buffer_descriptor_kind : std::uint8_t {
		storage,
		uniform,
		acceleration_structure,
	};

	struct descriptor_heap_properties {
		device_size sampler_heap_alignment = 0;
		device_size resource_heap_alignment = 0;
		device_size max_sampler_heap_size = 0;
		device_size max_resource_heap_size = 0;
		device_size min_sampler_heap_reserved_range = 0;
		device_size min_resource_heap_reserved_range = 0;
		device_size sampler_descriptor_size = 0;
		device_size image_descriptor_size = 0;
		device_size buffer_descriptor_size = 0;
		device_size acceleration_structure_descriptor_size = 0;
		device_size sampler_descriptor_alignment = 0;
		device_size image_descriptor_alignment = 0;
		device_size buffer_descriptor_alignment = 0;
		device_size max_push_data_size = 0;
		std::uint32_t max_embedded_samplers = 0;
		bool sparse_descriptor_heaps = false;
	};

	struct bindless_heap_binding {
		device_address address = 0;
		device_size size = 0;
		device_size reserved_offset = 0;
		device_size reserved_size = 0;
	};

	struct bindless_slot_pool {
		device_size base_offset = 0;
		device_size stride = 0;
		device_size base_index = 0;
		std::vector<std::uint32_t> free_list;
		std::mutex mutex;

		auto reset(
			std::uint32_t capacity
		) -> void;

		[[nodiscard]] auto allocate() -> bindless_slot;

		auto release(
			bindless_slot slot
		) -> void;

		[[nodiscard]] auto offset(
			bindless_slot slot
		) const -> device_size;
	};

	class bindless_handle final : public non_copyable {
	public:
		bindless_handle() {}

		bindless_handle(
			bindless_slot_pool* pool,
			bindless_slot slot
		);

		~bindless_handle();

		bindless_handle(
			bindless_handle&& other
		) noexcept;

		auto operator=(
			bindless_handle&& other
		) noexcept -> bindless_handle&;

		[[nodiscard]] auto slot() const -> bindless_slot;

		[[nodiscard]] auto valid() const -> bool;

	private:
		bindless_slot_pool* m_pool = nullptr;
		bindless_slot m_slot;
	};
}

auto gse::gpu::bindless_slot_pool::reset(const std::uint32_t capacity) -> void {
	free_list.clear();
	free_list.reserve(capacity);
	for (std::uint32_t i = 0; i < capacity; ++i) {
		free_list.push_back(capacity - 1 - i);
	}
}

auto gse::gpu::bindless_slot_pool::allocate() -> bindless_slot {
	std::lock_guard lock(mutex);
	assert(!free_list.empty(), "bindless_slot_pool exhausted");
	const auto index = free_list.back();
	free_list.pop_back();
	return {
		.index = static_cast<std::uint32_t>(base_index) + index
	};
}

auto gse::gpu::bindless_slot_pool::release(const bindless_slot slot) -> void {
	std::lock_guard lock(mutex);
	free_list.push_back(slot.index - static_cast<std::uint32_t>(base_index));
}

auto gse::gpu::bindless_slot_pool::offset(const bindless_slot slot) const -> device_size {
	return base_offset + (slot.index - base_index) * stride;
}

gse::gpu::bindless_handle::bindless_handle(bindless_slot_pool* pool, const bindless_slot slot)
	: m_pool(pool), m_slot(slot) {
}

gse::gpu::bindless_handle::~bindless_handle() {
	if (m_pool) {
		m_pool->release(m_slot);
	}
}

gse::gpu::bindless_handle::bindless_handle(bindless_handle&& other) noexcept
	: m_pool(other.m_pool), m_slot(other.m_slot) {
	other.m_pool = nullptr;
	other.m_slot = {};
}

auto gse::gpu::bindless_handle::operator=(bindless_handle&& other) noexcept -> bindless_handle& {
	if (this != &other) {
		if (m_pool) {
			m_pool->release(m_slot);
		}
		m_pool = other.m_pool;
		m_slot = other.m_slot;
		other.m_pool = nullptr;
		other.m_slot = {};
	}
	return *this;
}

auto gse::gpu::bindless_handle::slot() const -> bindless_slot {
	return m_slot;
}

auto gse::gpu::bindless_handle::valid() const -> bool {
	return m_pool != nullptr;
}
