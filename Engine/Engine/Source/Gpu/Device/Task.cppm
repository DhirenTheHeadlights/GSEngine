export module gse.gpu:gpu_task;

import std;

import :frame_resource_bin;
import :handles;
import :transient_queue;
import :types;
import :vulkan_device;
import :vulkan_queues;
import :vulkan_transient_command_buffer;

import gse.concurrency;
import gse.core;

export namespace gse::gpu {
    template <typename T = void>
    using gpu_task = async::task<T>;

    struct begin_transient_awaiter {
        const vulkan::device* m_device;
        transient_queue* m_queue;

        auto await_ready(
        ) const noexcept -> bool;

        auto await_suspend(
            std::coroutine_handle<>
        ) noexcept -> bool;

        auto await_resume(
        ) -> vulkan::transient_command_buffer;
    };

    class submission {
    public:
        submission(
            const vulkan::device& device,
            vulkan::queue& queues,
            transient_queue& queue,
            frame_resource_bin& bin,
            vulkan::transient_command_buffer&& cmd
        );

        template <typename T>
        auto retain(
            T&& resource
        ) && -> submission&&;

        auto await_ready(
        ) noexcept -> bool;

        auto await_suspend(
            std::coroutine_handle<> caller
        ) -> bool;

        auto await_resume(
        ) noexcept -> void;

    private:
        const vulkan::device* m_device;
        vulkan::queue* m_queues;
        transient_queue* m_queue;
        frame_resource_bin* m_bin;
        vulkan::transient_command_buffer m_cmd;
        std::vector<move_only_function<void()>> m_pending_retains;
        std::uint64_t m_value = 0;
        bool m_submitted = false;
    };
}

auto gse::gpu::begin_transient_awaiter::await_ready() const noexcept -> bool {
    return true;
}

auto gse::gpu::begin_transient_awaiter::await_suspend(std::coroutine_handle<>) noexcept -> bool {
    return false;
}

auto gse::gpu::begin_transient_awaiter::await_resume() -> vulkan::transient_command_buffer {
    auto cmd = m_queue->allocate_primary(*m_device);
    cmd.begin_one_time();
    return cmd;
}

gse::gpu::submission::submission(const vulkan::device& device, vulkan::queue& queues, transient_queue& queue, frame_resource_bin& bin, vulkan::transient_command_buffer&& cmd)
    : m_device(&device), m_queues(&queues), m_queue(&queue), m_bin(&bin), m_cmd(std::move(cmd)) {}

template <typename T>
auto gse::gpu::submission::retain(T&& resource) && -> submission&& {
    m_pending_retains.push_back([bin = m_bin, qid = m_queue->id(), value = std::ref(m_value), payload = std::forward<T>(resource)]() mutable {
        bin->retain(qid, value.get(), std::move(payload));
    });
    return std::move(*this);
}

auto gse::gpu::submission::await_ready() noexcept -> bool {
    return false;
}

auto gse::gpu::submission::await_suspend(std::coroutine_handle<> caller) -> bool {
    m_cmd.end();

    m_value = m_queue->reserve_value();

    const semaphore_submit_info signal{
        .semaphore = m_queue->timeline_handle(),
        .value = m_value,
        .stages = pipeline_stage_flag::all_commands,
    };
    const command_buffer_submit_info cmd_info{
        .command_buffer = m_cmd.handle(),
    };
    const submit_info info{
        .command_buffers = std::span(&cmd_info, 1),
        .signal_semaphores = std::span(&signal, 1),
    };

    if (m_queue->id() == queue_id::graphics) {
        m_queues->submit_graphics(info);
    }
    else {
        m_queues->submit_compute(info);
    }
    m_submitted = true;

    for (auto& fn : m_pending_retains) {
        fn();
    }
    m_pending_retains.clear();

    m_bin->retain(m_queue->id(), m_value, std::move(m_cmd));

    if (m_queue->reached(m_value)) {
        return false;
    }

    m_queue->park(m_value, caller);
    return true;
}

auto gse::gpu::submission::await_resume() noexcept -> void {
}
