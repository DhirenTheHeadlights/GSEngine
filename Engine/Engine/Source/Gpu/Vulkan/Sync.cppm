export module gse.gpu:vulkan_sync;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
    constexpr std::uint32_t max_frames_in_flight = 2;

    class sync : public non_copyable {
    public:
        ~sync(
        ) = default;

        sync(
            sync&&
        ) noexcept = default;

        auto operator=(
            sync&&
        ) noexcept -> sync& = default;

        [[nodiscard]] static auto create(
            const device& dev,
            std::uint32_t image_count,
            std::uint32_t frames_in_flight = max_frames_in_flight
        ) -> sync;

        [[nodiscard]] auto image_available(
            std::uint32_t frame_index
        ) const -> gpu::handle<semaphore>;

        [[nodiscard]] auto render_finished(
            std::uint32_t image_index
        ) const -> gpu::handle<semaphore>;

        [[nodiscard]] auto in_flight_fence(
            std::uint32_t frame_index
        ) const -> gpu::handle<fence>;

    private:
        sync(
            std::vector<vk::raii::Semaphore>&& image_available_semaphores,
            std::vector<vk::raii::Semaphore>&& render_finished_semaphores,
            std::vector<vk::raii::Fence>&& in_flight_fences
        );

        std::vector<vk::raii::Semaphore> m_image_available;
        std::vector<vk::raii::Semaphore> m_render_finished;
        std::vector<vk::raii::Fence> m_in_flight;
    };
}

gse::vulkan::sync::sync(std::vector<vk::raii::Semaphore>&& image_available_semaphores, std::vector<vk::raii::Semaphore>&& render_finished_semaphores, std::vector<vk::raii::Fence>&& in_flight_fences)
    : m_image_available(std::move(image_available_semaphores)),
      m_render_finished(std::move(render_finished_semaphores)),
      m_in_flight(std::move(in_flight_fences)) {}

auto gse::vulkan::sync::create(const device& dev, const std::uint32_t image_count, const std::uint32_t frames_in_flight) -> sync {
    std::vector<vk::raii::Semaphore> image_available;
    std::vector<vk::raii::Semaphore> render_finished;
    std::vector<vk::raii::Fence> in_flight_fences;

    image_available.reserve(image_count);
    render_finished.reserve(image_count);
    in_flight_fences.reserve(frames_in_flight);

    constexpr vk::SemaphoreCreateInfo bin_sem_ci{};
    constexpr vk::FenceCreateInfo fence_ci{
        .flags = vk::FenceCreateFlagBits::eSignaled,
    };

    for (std::uint32_t i = 0; i < image_count; ++i) {
        image_available.emplace_back(dev.raii_device(), bin_sem_ci);
        render_finished.emplace_back(dev.raii_device(), bin_sem_ci);
    }

    for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
        in_flight_fences.emplace_back(dev.raii_device(), fence_ci);
    }

    return sync(
        std::move(image_available),
        std::move(render_finished),
        std::move(in_flight_fences)
    );
}

auto gse::vulkan::sync::image_available(const std::uint32_t frame_index) const -> gpu::handle<semaphore> {
    return std::bit_cast<gpu::handle<semaphore>>(*m_image_available[frame_index]);
}

auto gse::vulkan::sync::render_finished(const std::uint32_t image_index) const -> gpu::handle<semaphore> {
    return std::bit_cast<gpu::handle<semaphore>>(*m_render_finished[image_index]);
}

auto gse::vulkan::sync::in_flight_fence(const std::uint32_t frame_index) const -> gpu::handle<fence> {
    return std::bit_cast<gpu::handle<fence>>(*m_in_flight[frame_index]);
}
