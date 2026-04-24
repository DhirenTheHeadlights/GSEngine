export module gse.gpu:gpu_device;

import std;
import vulkan;

import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;
import :descriptor_buffer_types;
import :gpu_command_pools;
import :gpu_types;
import :vulkan_reflect;
import :device_bootstrap;

import gse.os;
import gse.core;
import gse.save;

export namespace gse::gpu {
    class device final : public non_copyable {
    public:
        [[nodiscard]] static auto create(
            const window& win,
            save::state& save
        ) -> std::unique_ptr<device>;

        ~device() override;

        [[nodiscard]] auto logical_device(
            this auto&& self
        ) -> decltype(auto);

        [[nodiscard]] auto physical_device(
        ) -> const vk::raii::PhysicalDevice&;

        [[nodiscard]] auto allocator(
        ) -> vulkan::allocator&;

        [[nodiscard]] auto allocator(
        ) const -> const vulkan::allocator&;

        [[nodiscard]] auto descriptor_heap(
            this auto& self
        ) -> decltype(auto);

        [[nodiscard]] auto surface_format(
        ) const -> vk::Format;

        [[nodiscard]] auto compute_queue_family(
        ) const -> std::uint32_t;

        [[nodiscard]] auto compute_queue(
        ) -> const vk::raii::Queue&;

        auto wait_idle(
        ) const -> void;

        auto report_device_lost(
            std::string_view operation
        ) -> void;

        [[nodiscard]] auto instance_config(
            this auto& self
        ) -> auto&;

        [[nodiscard]] auto device_config(
            this auto& self
        ) -> auto&;

        [[nodiscard]] auto queue_config(
        ) -> vulkan::queue_config&;

        [[nodiscard]] auto command_config(
        ) -> vulkan::command_config&;

        [[nodiscard]] auto worker_command_pools(
        ) -> vulkan::worker_command_pools&;

        [[nodiscard]] auto descriptor_buffer_props(
        ) const -> const vulkan::descriptor_buffer_properties&;

        [[nodiscard]] auto video_encode_enabled(
        ) const -> bool;

    private:
        explicit device(
            vulkan::bootstrap_result&& boot
        );

        vulkan::instance_config m_instance;
        vulkan::device_config m_device_config;
        std::unique_ptr<vulkan::allocator> m_allocator;
        vulkan::queue_config m_queue;
        vulkan::command_config m_command;
        vulkan::worker_command_pools m_worker_pools;
        std::unique_ptr<vulkan::descriptor_heap> m_descriptor_heap;
        vulkan::descriptor_buffer_properties m_descriptor_buffer_props;
        vk::Format m_surface_format;
        std::atomic<bool> m_device_lost_reported = false;
        bool m_video_encode_enabled = false;
    };
}

auto gse::gpu::device::logical_device(this auto&& self) -> decltype(auto) {
    return (self.m_device_config.device);
}

auto gse::gpu::device::descriptor_heap(this auto& self) -> decltype(auto) {
    return (*self.m_descriptor_heap);
}

auto gse::gpu::device::instance_config(this auto& self) -> auto& {
    return self.m_instance;
}

auto gse::gpu::device::device_config(this auto& self) -> auto& {
    return self.m_device_config;
}
