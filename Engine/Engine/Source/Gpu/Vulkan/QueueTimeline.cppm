export module gse.gpu:vulkan_queue_timeline;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
    class queue_timeline final : public non_copyable {
    public:
        queue_timeline(
        ) = default;

        ~queue_timeline(
        ) = default;

        queue_timeline(
            queue_timeline&&
        ) noexcept = default;

        auto operator=(
            queue_timeline&&
        ) noexcept -> queue_timeline& = default;

        [[nodiscard]] static auto create(
            const device& device
        ) -> queue_timeline;

        [[nodiscard]] auto handle(
        ) const -> gpu::handle<semaphore>;

        [[nodiscard]] auto read(
        ) const -> std::uint64_t;

        auto wait_until(
            const device& device,
            std::uint64_t value
        ) const -> void;

    private:
        explicit queue_timeline(
            vk::raii::Semaphore&& semaphore
        );

        vk::raii::Semaphore m_semaphore{ nullptr };
    };
}

gse::vulkan::queue_timeline::queue_timeline(vk::raii::Semaphore&& semaphore)
    : m_semaphore(std::move(semaphore)) {}

auto gse::vulkan::queue_timeline::create(const device& device) -> queue_timeline {
    const vk::SemaphoreTypeCreateInfo type_info{
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue = 0,
    };
    const vk::SemaphoreCreateInfo sem_info{
        .pNext = &type_info,
    };
    return queue_timeline(device.raii_device().createSemaphore(sem_info));
}

auto gse::vulkan::queue_timeline::handle() const -> gpu::handle<semaphore> {
    return std::bit_cast<gpu::handle<semaphore>>(*m_semaphore);
}

auto gse::vulkan::queue_timeline::read() const -> std::uint64_t {
    return m_semaphore.getCounterValue();
}

auto gse::vulkan::queue_timeline::wait_until(const device& device, const std::uint64_t value) const -> void {
    const vk::SemaphoreWaitInfo wait_info{
        .semaphoreCount = 1,
        .pSemaphores = &*m_semaphore,
        .pValues = &value,
    };
    (void)device.raii_device().waitSemaphores(wait_info, std::numeric_limits<std::uint64_t>::max());
}
