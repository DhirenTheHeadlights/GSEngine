export module gse.gpu:vulkan_allocation;

import std;

import :handles;

import gse.core;

export namespace gse::vulkan {
    struct sub_allocation {
        gpu::device_size offset;
        gpu::device_size size;
        bool in_use = false;
    };

    struct allocation_debug_info {
        std::source_location creation_location = std::source_location::current();
        std::string tag;
        std::uint64_t allocation_id = 0;
    };

    template <typename Allocator>
    class basic_allocation : non_copyable {
    public:
        basic_allocation() = default;

        basic_allocation(
            std::uint64_t memory,
            gpu::device_size size,
            gpu::device_size offset,
            void* mapped,
            sub_allocation* owner,
            Allocator* alloc,
            const Allocator* device,
            allocation_debug_info debug_info = {}
        );

        ~basic_allocation(
        ) override;

        basic_allocation(
            basic_allocation&& other
        ) noexcept;

        auto operator=(
            basic_allocation&& other
        ) noexcept -> basic_allocation&;

        [[nodiscard]] auto memory(
        ) const -> std::uint64_t;

        [[nodiscard]] auto size(
        ) const -> gpu::device_size;

        [[nodiscard]] auto offset(
        ) const -> gpu::device_size;

        [[nodiscard]] auto mapped(
        ) const -> std::byte*;

        [[nodiscard]] auto owner(
        ) const -> sub_allocation*;

        [[nodiscard]] auto device(
        ) const -> const Allocator*;

        [[nodiscard]] auto debug_info(
        ) const -> const allocation_debug_info&;

    private:
        std::uint64_t m_memory = 0;
        gpu::device_size m_size = 0;
        gpu::device_size m_offset = 0;
        void* m_mapped = nullptr;
        sub_allocation* m_owner = nullptr;
        Allocator* m_allocator = nullptr;
        const Allocator* m_device = nullptr;
        allocation_debug_info m_debug_info;
    };
}

template <typename Allocator>
gse::vulkan::basic_allocation<Allocator>::basic_allocation(const std::uint64_t memory, const gpu::device_size size, const gpu::device_size offset, void* mapped, sub_allocation* owner, Allocator* alloc, const Allocator* device, allocation_debug_info debug_info)
    : m_memory(memory),
      m_size(size),
      m_offset(offset),
      m_mapped(mapped),
      m_owner(owner),
      m_allocator(alloc),
      m_device(device),
      m_debug_info(std::move(debug_info)) {}

template <typename Allocator>
gse::vulkan::basic_allocation<Allocator>::~basic_allocation() {
    if (m_owner && m_allocator) {
        m_allocator->free_allocation(*this);
    }
}

template <typename Allocator>
gse::vulkan::basic_allocation<Allocator>::basic_allocation(basic_allocation&& other) noexcept
    : m_memory(other.m_memory),
      m_size(other.m_size),
      m_offset(other.m_offset),
      m_mapped(other.m_mapped),
      m_owner(other.m_owner),
      m_allocator(other.m_allocator),
      m_device(other.m_device),
      m_debug_info(std::move(other.m_debug_info)) {
    other.m_owner = nullptr;
    other.m_allocator = nullptr;
    other.m_device = nullptr;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::operator=(basic_allocation&& other) noexcept -> basic_allocation& {
    if (this != &other) {
        if (m_owner && m_allocator) {
            m_allocator->free_allocation(*this);
        }

        m_memory = other.m_memory;
        m_size = other.m_size;
        m_offset = other.m_offset;
        m_mapped = other.m_mapped;
        m_owner = other.m_owner;
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_debug_info = std::move(other.m_debug_info);

        other.m_owner = nullptr;
        other.m_allocator = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::memory() const -> std::uint64_t {
    return m_memory;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::size() const -> gpu::device_size {
    return m_size;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::offset() const -> gpu::device_size {
    return m_offset;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::mapped() const -> std::byte* {
    return static_cast<std::byte*>(m_mapped);
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::owner() const -> sub_allocation* {
    return m_owner;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::device() const -> const Allocator* {
    return m_device;
}

template <typename Allocator>
auto gse::vulkan::basic_allocation<Allocator>::debug_info() const -> const allocation_debug_info& {
    return m_debug_info;
}
