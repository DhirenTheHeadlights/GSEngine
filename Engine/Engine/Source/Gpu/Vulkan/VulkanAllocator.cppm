export module gse.gpu:vulkan_allocator;

import std;
import vulkan;

import gse.core;
import gse.save;

export namespace gse::vulkan {
    class allocator;

    struct sub_allocation {
        vk::DeviceSize offset;
        vk::DeviceSize size;
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

        allocation(
            vk::DeviceMemory memory,
            vk::DeviceSize size,
            vk::DeviceSize offset,
            void* mapped,
            sub_allocation* owner,
            allocator* alloc,
            vk::Device device,
            allocation_debug_info debug_info = {}
        );

        ~allocation(
        ) override;

        allocation(
            allocation&& other
        ) noexcept;

        auto operator=(
            allocation&& other
        ) noexcept -> allocation&;

        [[nodiscard]] auto memory(
        ) const -> vk::DeviceMemory;

        [[nodiscard]] auto size(
        ) const -> vk::DeviceSize;

        [[nodiscard]] auto offset(
        ) const -> vk::DeviceSize;

        [[nodiscard]] auto mapped(
        ) const -> std::byte*;

        [[nodiscard]] auto owner(
        ) const -> sub_allocation*;

        [[nodiscard]] auto device(
        ) const -> vk::Device;

        [[nodiscard]] auto debug_info(
        ) const -> const allocation_debug_info&;

    private:
        vk::DeviceMemory m_memory = nullptr;
        vk::DeviceSize m_size = 0;
        vk::DeviceSize m_offset = 0;
        void* m_mapped = nullptr;
        sub_allocation* m_owner = nullptr;
        allocator* m_allocator = nullptr;
        vk::Device m_device = nullptr;
        allocation_debug_info m_debug_info;
    };

    struct image_resource_info {
        vk::Image image = nullptr;
        vk::ImageView view = nullptr;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;
        allocation allocation;
    };

    struct image_resource : non_copyable, image_resource_info {
        image_resource() = default;

        image_resource(
            vk::Image image,
            vk::ImageView view,
            vk::Format format,
            vk::ImageLayout current_layout,
            vulkan::allocation allocation
        );

        ~image_resource() override;

        image_resource(
            image_resource&& other
        ) noexcept;

        auto operator=(
            image_resource&& other
        ) noexcept -> image_resource&;
    };

    struct buffer_resource_info {
        vk::Buffer buffer = nullptr;
        vk::DeviceSize size = 0;
        allocation allocation;
    };

    struct buffer_resource : non_copyable, buffer_resource_info {
        buffer_resource() = default;

        buffer_resource(
            vk::Buffer buffer,
            vulkan::allocation allocation,
            vk::DeviceSize size
        );

        ~buffer_resource() override;

        buffer_resource(
            buffer_resource&& other
        ) noexcept;

        auto operator=(
            buffer_resource&& other
        ) noexcept -> buffer_resource&;
    };

    class allocator : non_copyable {
    public:
        allocator(
            const vk::raii::Device& device,
            const vk::raii::PhysicalDevice& physical_device,
            save::state& save_state
        );

        ~allocator() override;

        allocator(
            allocator&&
        ) noexcept;

        auto operator=(
            allocator&&
        ) noexcept -> allocator&;

        auto allocate(
            const vk::MemoryRequirements& requirements,
            vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            std::string_view tag = "",
            std::source_location loc = std::source_location::current(),
            bool device_address = false
        ) -> std::expected<allocation, std::string>;

        auto create_buffer(
            const vk::BufferCreateInfo& buffer_info,
            const void* data = nullptr,
            std::string_view tag = "",
            const std::source_location& loc = std::source_location::current()
        ) -> buffer_resource;

        auto create_image(
            const vk::ImageCreateInfo& info,
            vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            const vk::ImageViewCreateInfo& view_info = {},
            const void* data = nullptr,
            std::string_view tag = "",
            std::source_location loc = std::source_location::current()
        ) -> image_resource;

        auto clean_up(
        ) -> void;

        [[nodiscard]] auto live_allocation_count(
        ) const -> std::uint32_t;

        [[nodiscard]] auto tracking_enabled(
        ) const -> bool;

    private:
        friend class allocation;

        auto free(
            const allocation& alloc
        ) -> void;

        static auto memory_flag_preferences(
            vk::BufferUsageFlags usage
        ) -> std::vector<vk::MemoryPropertyFlags>;

        struct memory_block {
            vk::DeviceMemory memory;
            vk::DeviceSize size;
            vk::MemoryPropertyFlags properties;
            std::list<sub_allocation> allocations;
            void* mapped = nullptr;
        };

        struct pool {
            std::uint32_t memory_type_index;
            std::list<memory_block> blocks;
        };

        struct pool_key {
            std::uint32_t memory_type_index;
            vk::MemoryPropertyFlags properties;
            bool device_address = false;

            auto operator==(
                const pool_key& other
            ) const -> bool;
        };

        struct pool_key_hash {
            auto operator()(
                const pool_key& key
            ) const noexcept -> std::size_t;
        };

        static constexpr vk::DeviceSize k_default_block_size = 1024 * 1024 * 64;

        vk::Device m_device;
        vk::PhysicalDevice m_physical_device;
        std::unordered_map<pool_key, pool, pool_key_hash> m_pools;
        std::mutex m_mutex;

        std::atomic<std::uint32_t> m_live_allocation_count = 0;
        std::atomic<std::uint64_t> m_next_allocation_id = 1;
        bool m_cleaned_up = false;
        bool m_tracking_enabled = false;
        bool m_name_resources = false;
        std::unordered_map<std::uint64_t, allocation_debug_info> m_live_allocations;
    };
}
