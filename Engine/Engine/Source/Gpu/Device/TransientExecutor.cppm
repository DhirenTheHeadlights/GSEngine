export module gse.gpu:transient_executor;

import std;

import :frame_recorder;
import :frame_resource_bin;
import :handles;
import :vulkan_device;
import :transient_queue;

import gse.concurrency;
import gse.core;

export namespace gse::gpu {
    class transient_executor final : public non_copyable {
    public:
        ~transient_executor(
        );

        transient_executor(
            transient_executor&&
        ) noexcept = default;

        auto operator=(
            transient_executor&&
        ) noexcept -> transient_executor& = default;

        [[nodiscard]] static auto create(
            const vulkan::device& device,
            std::uint32_t graphics_family,
            std::uint32_t compute_family
        ) -> transient_executor;

        [[nodiscard]] auto recorder(
            this auto& self
        ) -> auto&;

        [[nodiscard]] auto bin(
            this auto& self
        ) -> auto&;

        [[nodiscard]] auto queue(
            queue_id id
        ) -> transient_queue&;

        auto detach(
            async::task<> task
        ) -> void;

        auto begin_frame(
            const vulkan::device& device
        ) -> void;

        auto wait_idle(
            const vulkan::device& device
        ) -> void;

    private:
        transient_executor(
            transient_queue&& graphics,
            transient_queue&& compute
        );

        frame_recorder m_recorder;
        frame_resource_bin m_bin;
        transient_queue m_graphics;
        transient_queue m_compute;
        std::vector<async::task<>> m_detached;
    };
}

gse::gpu::transient_executor::transient_executor(transient_queue&& graphics, transient_queue&& compute)
    : m_graphics(std::move(graphics)), m_compute(std::move(compute)) {}

gse::gpu::transient_executor::~transient_executor() = default;

auto gse::gpu::transient_executor::create(const vulkan::device& device, const std::uint32_t graphics_family, const std::uint32_t compute_family) -> transient_executor {
    auto graphics = transient_queue::create(device, queue_id::graphics, graphics_family);
    auto compute = transient_queue::create(device, queue_id::compute, compute_family);
    return transient_executor(std::move(graphics), std::move(compute));
}

auto gse::gpu::transient_executor::recorder(this auto& self) -> auto& {
    return self.m_recorder;
}

auto gse::gpu::transient_executor::bin(this auto& self) -> auto& {
    return self.m_bin;
}

auto gse::gpu::transient_executor::queue(const queue_id id) -> transient_queue& {
    switch (id) {
        case queue_id::graphics: {
            return m_graphics;
        }
        case queue_id::compute: {
            return m_compute;
        }
    }
    return m_graphics;
}

auto gse::gpu::transient_executor::detach(async::task<> task) -> void {
    m_detached.push_back(std::move(task));
}

auto gse::gpu::transient_executor::begin_frame(const vulkan::device& device) -> void {
    const auto graphics_progress = m_graphics.poll(device);
    const auto compute_progress = m_compute.poll(device);

    const std::array<queue_progress, queue_id_count> progress{
        queue_progress{
            .queue = queue_id::graphics,
            .reached_value = graphics_progress,
        },
        queue_progress{
            .queue = queue_id::compute,
            .reached_value = compute_progress,
        },
    };

    m_bin.drain(progress);

    std::erase_if(m_detached, [](const async::task<>& t) {
        return t.done();
    });
}

auto gse::gpu::transient_executor::wait_idle(const vulkan::device& device) -> void {
    m_graphics.wait_idle(device);
    m_compute.wait_idle(device);
    m_detached.clear();
    m_bin.wait_idle_clear();
    m_recorder.clear();
}
