export module gse.gpu:vulkan_device;

import std;
import vulkan;

import :vulkan_allocation;
import :vulkan_buffer;
import :vulkan_image;
import :vulkan_commands;
import :vulkan_instance;
import :vulkan_queues;
import :types;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.save;
import gse.log;

export namespace gse::vulkan {
    class device;

    auto transition_image_layout(
        basic_image<device>& image_resource,
        gpu::handle<command_buffer> cmd_handle,
        gpu::image_layout new_layout,
        gpu::image_aspect_flags aspects,
        gpu::pipeline_stage_flags src_stages,
        gpu::access_flags src_access,
        gpu::pipeline_stage_flags dst_stages,
        gpu::access_flags dst_access,
        std::uint32_t mip_levels = 1,
        std::uint32_t layer_count = 1
    ) -> void;

    [[nodiscard]] auto query_descriptor_buffer_properties(
        const vk::raii::PhysicalDevice& physical_device
    ) -> gpu::descriptor_buffer_properties;

    [[nodiscard]] auto wait_for_fence(
        const device& dev,
        gpu::handle<fence> fence,
        std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
    ) -> gpu::result;

    auto reset_fence(
        const device& dev,
        gpu::handle<fence> fence
    ) -> void;

    struct acquire_next_image_result {
        gpu::result result = gpu::result::error_unknown;
        std::uint32_t image_index = 0;
    };

    [[nodiscard]] auto acquire_next_image(
        const device& dev,
        gpu::handle<swap_chain> swapchain,
        gpu::handle<semaphore> wait_semaphore,
        std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
    ) -> acquire_next_image_result;

    struct device_creation_result;

    class device : public non_copyable {
    public:
        ~device(
        ) override;

        device(
            device&&
        ) noexcept;

        auto operator=(
            device&&
        ) noexcept -> device&;

        [[nodiscard]] static auto create(
            const instance& instance_data,
            save::state& save
        ) -> device_creation_result;

        [[nodiscard]] auto physical_device(
            this auto&& self
        ) -> auto&;

        [[nodiscard]] auto raii_device(
            this auto&& self
        ) -> auto&;

        [[nodiscard]] auto device_handle(
        ) const -> gpu::handle<device>;

        [[nodiscard]] auto fault_enabled(
        ) const -> bool;

        [[nodiscard]] auto vendor_binary_fault_enabled(
        ) const -> bool;

        auto wait_idle(
        ) const -> void;

        [[nodiscard]] auto timestamp_period(
        ) const -> float;

        [[nodiscard]] auto query_fault_counts(
            gpu::device_fault_counts& counts
        ) const -> gpu::result;

        [[nodiscard]] auto query_fault_info(
            gpu::device_fault_counts& counts,
            gpu::device_fault_info& info
        ) const -> gpu::result;

        auto create_buffer(
            const vk::BufferCreateInfo& buffer_info,
            const void* data = nullptr,
            std::string_view tag = "",
            const std::source_location& loc = std::source_location::current()
        ) -> basic_buffer<device>;

        auto create_buffer(
            const gpu::buffer_create_info& buffer_info,
            const void* data = nullptr,
            std::string_view tag = "",
            const std::source_location& loc = std::source_location::current()
        ) -> basic_buffer<device>;

        auto create_image(
            const vk::ImageCreateInfo& info,
            vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            const vk::ImageViewCreateInfo& view_info = {},
            const void* data = nullptr,
            std::string_view tag = "",
            std::source_location loc = std::source_location::current()
        ) -> basic_image<device>;

        auto create_image(
            const gpu::image_create_info& info,
            gpu::memory_property_flags properties = gpu::memory_property_flag::device_local,
            const gpu::image_view_create_info& view_info = {},
            const void* data = nullptr,
            std::string_view tag = "",
            std::source_location loc = std::source_location::current()
        ) -> basic_image<device>;

        [[nodiscard]] auto live_allocation_count(
        ) const -> std::uint32_t;

        [[nodiscard]] auto tracking_enabled(
        ) const -> bool;

        auto destroy_buffer(
            gpu::handle<buffer> buffer
        ) const -> void;

        auto destroy_image(
            gpu::handle<image> image
        ) const -> void;

        auto destroy_image_view(
            gpu::handle<image_view> view
        ) const -> void;

        auto free_allocation(
            const basic_allocation<device>& alloc
        ) -> void;

    private:
        device(
            vk::raii::PhysicalDevice&& physical_device,
            vk::raii::Device&& device,
            save::state& save_state,
            bool device_fault_enabled,
            bool device_fault_vendor_binary_enabled
        );

        auto allocate(
            const vk::MemoryRequirements& requirements,
            vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            std::string_view tag = "",
            std::source_location loc = std::source_location::current(),
            bool device_address = false
        ) -> std::expected<basic_allocation<device>, std::string>;

        auto clean_up(
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

        vk::raii::PhysicalDevice m_physical_device;
        vk::raii::Device m_device;
        bool m_fault_enabled = false;
        bool m_vendor_binary_fault_enabled = false;

        std::unordered_map<pool_key, pool, pool_key_hash> m_pools;
        std::mutex m_mutex;

        std::atomic<std::uint32_t> m_live_allocation_count = 0;
        std::atomic<std::uint64_t> m_next_allocation_id = 1;
        bool m_cleaned_up = false;
        bool m_tracking_enabled = false;
        bool m_name_resources = false;
        std::unordered_map<std::uint64_t, allocation_debug_info> m_live_allocations;
    };

    struct device_creation_result {
        device device;
        queue queue;
        queue_family families;
        bool video_encode_enabled = false;
        gpu::descriptor_buffer_properties desc_buf_props;
    };
}

auto gse::vulkan::device::physical_device(this auto&& self) -> auto& {
    return self.m_physical_device;
}

auto gse::vulkan::device::raii_device(this auto&& self) -> auto& {
    return self.m_device;
}
