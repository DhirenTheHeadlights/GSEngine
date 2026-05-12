export module gse.gpu:transient_queue;

import std;

import :frame_resource_bin;
import :handles;
import :vulkan_device;
import :vulkan_queue_timeline;
import :vulkan_transient_command_buffer;
import :vulkan_transient_command_pool;
import :vulkan_semaphore;

import gse.assert;
import gse.core;

export namespace gse::gpu {
    class transient_queue final : public non_copyable {
    public:
        struct submit_ticket {
            std::unique_lock<std::mutex> lock;
            std::uint64_t value = 0;
        };

        ~transient_queue(
        ) = default;

        transient_queue(
            transient_queue&& other
        ) noexcept;

        auto operator=(
            transient_queue&& other
        ) noexcept -> transient_queue&;

        [[nodiscard]] static auto create(
            const vulkan::device& device,
            queue_id id,
            std::uint32_t family,
            std::size_t worker_count
        ) -> transient_queue;

        [[nodiscard]] auto id(
        ) const -> queue_id;

        [[nodiscard]] auto reserve_for_submit(
        ) -> submit_ticket;

        [[nodiscard]] auto progress(
        ) const -> std::uint64_t;

        [[nodiscard]] auto reached(
            std::uint64_t value
        ) const -> bool;

        [[nodiscard]] auto timeline_handle(
        ) const -> handle<vulkan::semaphore>;

        [[nodiscard]] auto allocate_primary(
            const vulkan::device& device,
            std::size_t worker_idx
        ) -> vulkan::transient_command_buffer;

        auto park(
            std::uint64_t value,
            std::coroutine_handle<> handle
        ) -> void;

        auto poll(
            const vulkan::device& device
        ) -> std::uint64_t;

        auto wait_until(
            const vulkan::device& device,
            std::uint64_t value
        ) -> void;

        auto wait_idle(
            const vulkan::device& device
        ) -> void;

    private:
        transient_queue(
            queue_id id,
            vulkan::queue_timeline&& timeline,
            std::vector<vulkan::transient_command_pool>&& pools
        );

        struct waiter {
            std::uint64_t m_value;
            std::coroutine_handle<> m_handle;
        };

        queue_id m_id;
        vulkan::queue_timeline m_timeline;
        std::vector<vulkan::transient_command_pool> m_pools;
        std::uint64_t m_next_value = 0;
        std::atomic<std::uint64_t> m_progress{ 0 };
        std::vector<waiter> m_waiters;
        std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
    };
}

gse::gpu::transient_queue::transient_queue(queue_id id, vulkan::queue_timeline&& timeline, std::vector<vulkan::transient_command_pool>&& pools)
    : m_id(id), m_timeline(std::move(timeline)), m_pools(std::move(pools)) {}

gse::gpu::transient_queue::transient_queue(transient_queue&& other) noexcept
    : m_id(other.m_id)
    , m_timeline(std::move(other.m_timeline))
    , m_pools(std::move(other.m_pools))
    , m_next_value(other.m_next_value)
    , m_progress(other.m_progress.load(std::memory_order_relaxed))
    , m_waiters(std::move(other.m_waiters))
    , m_mutex(std::move(other.m_mutex)) {}

auto gse::gpu::transient_queue::operator=(transient_queue&& other) noexcept -> transient_queue& {
    if (this != &other) {
        m_id = other.m_id;
        m_timeline = std::move(other.m_timeline);
        m_pools = std::move(other.m_pools);
        m_next_value = other.m_next_value;
        m_progress.store(other.m_progress.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_waiters = std::move(other.m_waiters);
        m_mutex = std::move(other.m_mutex);
    }
    return *this;
}

auto gse::gpu::transient_queue::create(const vulkan::device& device, const queue_id id, const std::uint32_t family, const std::size_t worker_count) -> transient_queue {
    std::vector<vulkan::transient_command_pool> pools;
    pools.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        pools.push_back(vulkan::transient_command_pool::create(device, family));
    }
    return transient_queue(
        id,
        vulkan::queue_timeline::create(device),
        std::move(pools)
    );
}

auto gse::gpu::transient_queue::id() const -> queue_id {
    return m_id;
}

auto gse::gpu::transient_queue::reserve_for_submit() -> submit_ticket {
    std::unique_lock lock(*m_mutex);
    const auto value = ++m_next_value;
    return submit_ticket{ .lock = std::move(lock), .value = value };
}

auto gse::gpu::transient_queue::progress() const -> std::uint64_t {
    return m_progress.load(std::memory_order_acquire);
}

auto gse::gpu::transient_queue::reached(const std::uint64_t value) const -> bool {
    return m_progress.load(std::memory_order_acquire) >= value;
}

auto gse::gpu::transient_queue::timeline_handle() const -> handle<vulkan::semaphore> {
    return m_timeline.handle();
}

auto gse::gpu::transient_queue::allocate_primary(const vulkan::device& device, const std::size_t worker_idx) -> vulkan::transient_command_buffer {
    assert(worker_idx < m_pools.size(), "transient_queue: worker index {} out of range (pools: {})", worker_idx, m_pools.size());
    auto& pool = m_pools[worker_idx];
    pool.try_reset(m_progress.load(std::memory_order_acquire));
    return pool.allocate_primary(device);
}

auto gse::gpu::transient_queue::park(const std::uint64_t value, std::coroutine_handle<> handle) -> void {
    std::lock_guard lock(*m_mutex);
    m_waiters.push_back({
        .m_value = value,
        .m_handle = handle,
    });
}

auto gse::gpu::transient_queue::poll(const vulkan::device& device) -> std::uint64_t {
    const auto reached_value = m_timeline.read();

    std::vector<std::coroutine_handle<>> ready;
    {
        std::lock_guard lock(*m_mutex);
        m_progress.store(reached_value, std::memory_order_release);
        std::erase_if(m_waiters, [&](const waiter& w) {
            if (w.m_value <= reached_value) {
                ready.push_back(w.m_handle);
                return true;
            }
            return false;
        });
    }

    for (auto handle : ready) {
        handle.resume();
    }

    return reached_value;
}

auto gse::gpu::transient_queue::wait_until(const vulkan::device& device, const std::uint64_t value) -> void {
    if (m_progress.load(std::memory_order_acquire) >= value) {
        return;
    }
    m_timeline.wait_until(device, value);

    std::vector<std::coroutine_handle<>> ready;
    {
        std::lock_guard lock(*m_mutex);
        const auto current = m_progress.load(std::memory_order_relaxed);
        if (value > current) {
            m_progress.store(value, std::memory_order_release);
        }
        const auto progress_snapshot = m_progress.load(std::memory_order_relaxed);
        std::erase_if(m_waiters, [&](const waiter& w) {
            if (w.m_value <= progress_snapshot) {
                ready.push_back(w.m_handle);
                return true;
            }
            return false;
        });
    }
    for (auto handle : ready) {
        handle.resume();
    }
}

auto gse::gpu::transient_queue::wait_idle(const vulkan::device& device) -> void {
    std::uint64_t target;
    {
        std::lock_guard lock(*m_mutex);
        if (m_next_value == 0) {
            return;
        }
        target = m_next_value;
    }
    wait_until(device, target);

    for (auto& pool : m_pools) {
        pool.reset_all();
    }
}
