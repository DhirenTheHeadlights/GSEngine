export module gse.gpu:transient_api;

import std;

import :device;
import :frame_resource_bin;
import :gpu_task;
import :transient_executor;
import :vulkan_device;
import :vulkan_queues;
import :vulkan_transient_command_buffer;

import gse.concurrency;

export namespace gse::gpu {
    [[nodiscard]] auto begin_transient(
        gpu::device& dev,
        queue_id id
    ) -> begin_transient_awaiter;

    [[nodiscard]] auto submit(
        gpu::device& dev,
        vulkan::transient_command_buffer&& cmd,
        queue_id id
    ) -> submission;

    auto dispatch(
        gpu::device& dev,
        async::task<> task
    ) -> void;
}

auto gse::gpu::begin_transient(gpu::device& dev, const queue_id id) -> begin_transient_awaiter {
    return begin_transient_awaiter{
        .m_device = &dev.vulkan_device(),
        .m_queue = &dev.transient().queue(id),
    };
}

auto gse::gpu::submit(gpu::device& dev, vulkan::transient_command_buffer&& cmd, const queue_id id) -> submission {
    return submission(
        dev.vulkan_device(),
        dev.vulkan_queue(),
        dev.transient().queue(id),
        dev.transient().bin(),
        std::move(cmd)
    );
}

auto gse::gpu::dispatch(gpu::device& dev, async::task<> task) -> void {
    task.start();
    dev.transient().detach(std::move(task));
}
