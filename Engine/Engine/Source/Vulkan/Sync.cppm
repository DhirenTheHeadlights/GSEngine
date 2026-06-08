export module gse.vulkan:sync;

import std;
import vulkan;

import :handles;
import :types;
import :device;

import gse.core;
import gse.assert;

export namespace gse::vulkan {
	class sync : public non_copyable {
	public:
		~sync();

		sync(
			sync&& other
		) noexcept;

		auto operator=(
			sync&& other
		) noexcept -> sync&;

		[[nodiscard]]
		static auto create(
			device& dev,
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = gpu::max_frames_in_flight
		) -> sync;

		[[nodiscard]] auto image_available(
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto render_finished(
			std::uint32_t image_index
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]]
		auto in_flight_fence(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::fence>;

	private:
		sync(
			device& dev,
			std::vector<gpu::handle<gpu::semaphore>>&& image_available_semaphores,
			std::vector<gpu::handle<gpu::semaphore>>&& render_finished_semaphores,
			std::array<std::vector<vk::raii::Fence>, gpu::queue_type_count>&& in_flight_fences
		);

		device* m_device = nullptr;
		std::vector<gpu::handle<gpu::semaphore>> m_image_available;
		std::vector<gpu::handle<gpu::semaphore>> m_render_finished;
		std::array<std::vector<vk::raii::Fence>, gpu::queue_type_count> m_in_flight;
	};
}

gse::vulkan::sync::sync(device& dev, std::vector<gpu::handle<gpu::semaphore>>&& image_available_semaphores, std::vector<gpu::handle<gpu::semaphore>>&& render_finished_semaphores, std::array<std::vector<vk::raii::Fence>, gpu::queue_type_count>&& in_flight_fences)
	: m_device(&dev), m_image_available(std::move(image_available_semaphores)), m_render_finished(std::move(render_finished_semaphores)), m_in_flight(std::move(in_flight_fences)) {
}

gse::vulkan::sync::~sync() {
	if (m_device) {
		for (const auto handle : m_image_available) {
			if (handle) {
				m_device->retire(handle);
			}
		}
		for (const auto handle : m_render_finished) {
			if (handle) {
				m_device->retire(handle);
			}
		}
	}
}

gse::vulkan::sync::sync(sync&& other) noexcept
	: m_device(other.m_device), m_image_available(std::move(other.m_image_available)), m_render_finished(std::move(other.m_render_finished)), m_in_flight(std::move(other.m_in_flight)) {
	other.m_device = nullptr;
}

auto gse::vulkan::sync::operator=(sync&& other) noexcept -> sync& {
	if (this != &other) {
		if (m_device) {
			for (const auto handle : m_image_available) {
				if (handle) {
					m_device->retire(handle);
				}
			}
			for (const auto handle : m_render_finished) {
				if (handle) {
					m_device->retire(handle);
				}
			}
		}
		m_device = other.m_device;
		m_image_available = std::move(other.m_image_available);
		m_render_finished = std::move(other.m_render_finished);
		m_in_flight = std::move(other.m_in_flight);
		other.m_device = nullptr;
	}
	return *this;
}

auto gse::vulkan::sync::create(device& dev, const std::uint32_t image_count, const std::uint32_t frames_in_flight) -> sync {
	std::vector<gpu::handle<gpu::semaphore>> image_available;
	std::vector<gpu::handle<gpu::semaphore>> render_finished;
	std::array<std::vector<vk::raii::Fence>, gpu::queue_type_count> in_flight_fences;

	image_available.reserve(image_count);
	render_finished.reserve(image_count);

	constexpr vk::FenceCreateInfo fence_ci{
		.flags = vk::FenceCreateFlagBits::eSignaled,
	};

	for (std::uint32_t i = 0; i < image_count; ++i) {
		image_available.push_back(dev.create_semaphore());
		render_finished.push_back(dev.create_semaphore());
	}

	for (auto& fences : in_flight_fences) {
		fences.reserve(frames_in_flight);
		for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
			auto [result, fence] = dev.raii_device().createFence(fence_ci);
			assert(result == vk::Result::eSuccess, "failed to create in-flight fence: {}", vk::to_string(result));
			fences.push_back(std::move(fence));
		}
	}

	return sync(dev, std::move(image_available), std::move(render_finished), std::move(in_flight_fences));
}

auto gse::vulkan::sync::image_available(const std::uint32_t frame_index) const -> gpu::handle<gpu::semaphore> {
	return m_image_available[frame_index];
}

auto gse::vulkan::sync::render_finished(const std::uint32_t image_index) const -> gpu::handle<gpu::semaphore> {
	return m_render_finished[image_index];
}

auto gse::vulkan::sync::in_flight_fence(const gpu::queue_type queue, const std::uint32_t frame_index) const -> gpu::handle<gpu::fence> {
	return std::bit_cast<gpu::handle<gpu::fence>>(*m_in_flight[static_cast<std::size_t>(queue)][frame_index]);
}
