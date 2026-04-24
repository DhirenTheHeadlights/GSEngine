export module gse.gpu:gpu_frame;

import std;

import :vulkan_runtime;
import :gpu_device;
import :gpu_swapchain;
import :gpu_sync;
import :gpu_types;

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
            vulkan::sync_config&& sync,
            vulkan::frame_context_config&& frame_ctx,
            device& dev,
            swap_chain& sc
        );

        [[nodiscard]] auto current_frame(
        ) const -> std::uint32_t;

        [[nodiscard]] auto image_index(
        ) const -> std::uint32_t;

        [[nodiscard]] auto command_buffer(
        ) const -> vk::CommandBuffer;

        [[nodiscard]] auto frame_in_progress(
        ) const -> bool;

        auto begin(
            window& win
        ) -> std::expected<frame_token, frame_status>;

        auto end(
            window& win
        ) -> void;

        auto set_sync(
            vulkan::sync_config&& sync
        ) -> void;

        auto add_wait_semaphore(
            const compute_semaphore_state& state
        ) -> void;

        auto add_transient_work(
            auto&& commands
        ) -> void;

    private:
        auto recreate_resources(
            const window& win
        ) -> void;

        auto cleanup_finished(
            std::uint32_t frame_index
        ) -> void;

        static auto create_sync_objects(
            const vulkan::device_config& device_data,
            const vulkan::swap_chain_config& swap_chain_data
        ) -> vulkan::sync_config;

        vulkan::sync_config m_sync;
        vulkan::frame_context_config m_frame_context;
        std::uint32_t m_current_frame = 0;
        bool m_frame_in_progress = false;
        device* m_device;
        swap_chain* m_swapchain;
        std::vector<compute_semaphore_state> m_extra_waits;
        std::vector<std::vector<vulkan::transient_gpu_work>> m_graveyard;
    };
}

auto gse::gpu::frame::add_transient_work(auto&& commands) -> void {
    const auto& vk_device = m_device->logical_device();

    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *m_device->command_config().pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    auto buffers = vk_device.allocateCommandBuffers(alloc_info);
    vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

    constexpr vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };

    command_buffer.begin(begin_info);

    std::vector<vulkan::buffer_resource> transient_buffers;
    if constexpr (std::is_void_v<std::invoke_result_t<decltype(commands), vk::raii::CommandBuffer&>>) {
        std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
    } else {
        transient_buffers = std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
    }

    command_buffer.end();

    vk::raii::Fence fence = vk_device.createFence({});

    const vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer,
    };

    try {
        m_device->queue_config().graphics.submit(submit_info, *fence);
    } catch (const vk::DeviceLostError&) {
        m_device->report_device_lost("transient graphics submission");
        throw;
    }

    m_graveyard[m_current_frame].push_back({
        .command_buffer = std::move(command_buffer),
        .fence = std::move(fence),
        .transient_buffers = std::move(transient_buffers),
    });
}
