export module gse.gpu:frame;

import std;

import :transient_executor;
import :vulkan_command_pools;
import :vulkan_device;
import :vulkan_queues;
import :vulkan_swapchain;
import :vulkan_sync;
import :device;
import :swap_chain;
import :types;
import gse.os;
import gse.core;

export namespace gse::gpu {
    class frame final : public non_copyable {
    public:
        [[nodiscard]] static auto create(
            device& dev,
            swap_chain& sc
        ) -> std::unique_ptr<frame>;

        frame(
            vulkan::sync&& sync,
            std::uint32_t image_index,
            gpu::handle<vulkan::command_buffer> command_buffer,
            device& dev,
            swap_chain& sc
        );

        [[nodiscard]] auto current_frame(
        ) const -> std::uint32_t;

        [[nodiscard]] auto image_index(
        ) const -> std::uint32_t;

        [[nodiscard]] auto command_buffer(
        ) const -> gpu::handle<vulkan::command_buffer>;

        [[nodiscard]] auto frame_in_progress(
        ) const -> bool;

        auto begin(
            window& win
        ) -> std::expected<frame_token, frame_status>;

        auto end(
            window& win
        ) -> void;

        auto set_sync(
            vulkan::sync&& sync
        ) -> void;

        auto add_wait_semaphore(
            const compute_semaphore_state& state
        ) -> void;

    private:
        auto recreate_resources(
            const window& win
        ) -> void;

        static auto create_sync_objects(
            const vulkan::device& device_data,
            const vulkan::swap_chain& swap_chain_data
        ) -> vulkan::sync;

        vulkan::sync m_sync;
        std::uint32_t m_image_index = 0;
        gpu::handle<vulkan::command_buffer> m_command_buffer{};
        std::uint32_t m_current_frame = 0;
        bool m_frame_in_progress = false;
        device* m_device;
        swap_chain* m_swapchain;
        std::vector<compute_semaphore_state> m_extra_waits;
    };
}
