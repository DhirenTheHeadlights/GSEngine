export module gse.gpu:transient_queue;

import std;

import :frame_resource_bin;
import :handles;
import :vulkan_device;
import :vulkan_queue_timeline;
import :vulkan_transient_command_buffer;
import :vulkan_transient_command_pool;
import :vulkan_semaphore;

import gse.core;

export namespace gse::gpu {
    class transient_queue final : public non_copyable {
    public:
        ~transient_queue(
        ) = default;

        transient_queue(
            transient_queue&&
        ) noexcept = default;

        auto operator=(
            transient_queue&&
        ) noexcept -> transient_queue& = default;

        [[nodiscard]] static auto create(
            const vulkan::device& device,
            queue_id id,
            std::uint32_t family
        ) -> transient_queue;

        [[nodiscard]] auto id(
        ) const -> queue_id;

        [[nodiscard]] auto reserve_value(
        ) -> std::uint64_t;

        [[nodiscard]] auto progress(
        ) const -> std::uint64_t;

        [[nodiscard]] auto reached(
            std::uint64_t value
        ) const -> bool;

        [[nodiscard]] auto timeline_handle(
        ) const -> handle<vulkan::semaphore>;

        [[nodiscard]] auto allocate_primary(
            const vulkan::device& device
        ) -> vulkan::transient_command_buffer;

        auto park(
            std::uint64_t value,
            std::coroutine_handle<> handle
        ) -> void;

        auto poll(
            const vulkan::device& device
        ) -> std::uint64_t;

        auto wait_idle(
            const vulkan::device& device
        ) -> void;

    private:
        transient_queue(
            queue_id id,
            vulkan::queue_timeline&& timeline,
            vulkan::transient_command_pool&& pool
        );

        struct waiter {
            std::uint64_t m_value;
            std::coroutine_handle<> m_handle;
        };

        queue_id m_id;
        vulkan::queue_timeline m_timeline;
        vulkan::transient_command_pool m_pool;
        std::uint64_t m_next_value = 0;
        std::uint64_t m_progress = 0;
        std::vector<waiter> m_waiters;
    };
}

gse::gpu::transient_queue::transient_queue(queue_id id, vulkan::queue_timeline&& timeline, vulkan::transient_command_pool&& pool)
    : m_id(id), m_timeline(std::move(timeline)), m_pool(std::move(pool)) {}

auto gse::gpu::transient_queue::create(const vulkan::device& device, const queue_id id, const std::uint32_t family) -> transient_queue {
    return transient_queue(
        id,
        vulkan::queue_timeline::create(device),
        vulkan::transient_command_pool::create(device, family)
    );
}

auto gse::gpu::transient_queue::id() const -> queue_id {
    return m_id;
}

auto gse::gpu::transient_queue::reserve_value() -> std::uint64_t {
    return ++m_next_value;
}

auto gse::gpu::transient_queue::progress() const -> std::uint64_t {
    return m_progress;
}

auto gse::gpu::transient_queue::reached(const std::uint64_t value) const -> bool {
    return m_progress >= value;
}

auto gse::gpu::transient_queue::timeline_handle() const -> handle<vulkan::semaphore> {
    return m_timeline.handle();
}

auto gse::gpu::transient_queue::allocate_primary(const vulkan::device& device) -> vulkan::transient_command_buffer {
    return m_pool.allocate_primary(device);
}

auto gse::gpu::transient_queue::park(const std::uint64_t value, std::coroutine_handle<> handle) -> void {
    m_waiters.push_back({
        .m_value = value,
        .m_handle = handle,
    });
}

auto gse::gpu::transient_queue::poll(const vulkan::device& device) -> std::uint64_t {
    m_progress = m_timeline.read();

    std::vector<std::coroutine_handle<>> ready;
    std::erase_if(m_waiters, [&](const waiter& w) {
        if (w.m_value <= m_progress) {
            ready.push_back(w.m_handle);
            return true;
        }
        return false;
    });

    for (auto handle : ready) {
        handle.resume();
    }

    return m_progress;
}

auto gse::gpu::transient_queue::wait_idle(const vulkan::device& device) -> void {
    if (m_next_value == 0) {
        return;
    }
    m_timeline.wait_until(device, m_next_value);
    m_progress = m_next_value;

    std::vector<std::coroutine_handle<>> ready;
    for (const auto& w : m_waiters) {
        ready.push_back(w.m_handle);
    }
    m_waiters.clear();
    for (auto handle : ready) {
        handle.resume();
    }
}
