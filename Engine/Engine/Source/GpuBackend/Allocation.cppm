export module gse.gpu_backend:allocation;

import std;

import :core;

import gse.core;

export namespace gse::gpu {
	struct sub_allocation {
		device_size offset;
		device_size size;
		bool in_use = false;
	};

	struct allocation_debug_info {
		std::source_location creation_location = std::source_location::current();
		std::string tag;
		std::uint64_t allocation_id = 0;
	};

	class allocation : non_copyable {
	public:
		allocation() = default;

		~allocation() = default;

		allocation(
			allocation&&
		) noexcept = default;

		auto operator=(
			allocation&&
		) noexcept -> allocation& = default;

		allocation(
			std::uint64_t memory,
			device_size size,
			device_size offset,
			void* mapped,
			sub_allocation* owner,
			allocation_debug_info debug_info = {}
		);

		[[nodiscard]] auto memory() const -> std::uint64_t;

		[[nodiscard]] auto size() const -> device_size;

		[[nodiscard]] auto offset() const -> device_size;

		[[nodiscard]] auto mapped() const -> std::byte*;

		[[nodiscard]] auto owner() const -> sub_allocation*;

		[[nodiscard]] auto debug_info() const -> const allocation_debug_info&;

	private:
		std::uint64_t m_memory = 0;
		device_size m_size = 0;
		device_size m_offset = 0;
		void* m_mapped = nullptr;
		sub_allocation* m_owner = nullptr;
		allocation_debug_info m_debug_info;
	};
}

gse::gpu::allocation::allocation(const std::uint64_t memory, const device_size size, const device_size offset, void* mapped, sub_allocation* owner, allocation_debug_info debug_info)
	: m_memory(memory), m_size(size), m_offset(offset), m_mapped(mapped), m_owner(owner), m_debug_info(std::move(debug_info)) {
}

auto gse::gpu::allocation::memory() const -> std::uint64_t {
	return m_memory;
}

auto gse::gpu::allocation::size() const -> device_size {
	return m_size;
}

auto gse::gpu::allocation::offset() const -> device_size {
	return m_offset;
}

auto gse::gpu::allocation::mapped() const -> std::byte* {
	return static_cast<std::byte*>(m_mapped);
}

auto gse::gpu::allocation::owner() const -> sub_allocation* {
	return m_owner;
}

auto gse::gpu::allocation::debug_info() const -> const allocation_debug_info& {
	return m_debug_info;
}