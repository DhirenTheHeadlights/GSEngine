export module gse.gpu_backend:frame_sync;

import std;

import :core;
import :enums;

import gse.core;

export namespace gse::gpu {
	template <typename T>
	concept sync_device = requires(T& device, gpu::handle<gpu::semaphore> semaphore, gpu::handle<gpu::fence> fence, bool signaled) {
		{ device.create_semaphore() } -> std::same_as<gpu::handle<gpu::semaphore>>;
		{ device.create_fence(signaled) } -> std::same_as<gpu::handle<gpu::fence>>;
		device.retire(semaphore);
		device.retire(fence);
	};

	template <sync_device Device>
	class swapchain_sync final : public non_copyable {
	public:
		swapchain_sync() = default;
		~swapchain_sync();

		swapchain_sync(
			swapchain_sync&& other
		) noexcept;

		auto operator=(
			swapchain_sync&& other
		) noexcept -> swapchain_sync&;

		[[nodiscard]]
		static auto create(
			Device& device,
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = max_frames_in_flight
		) -> swapchain_sync;

		[[nodiscard]] auto image_available(
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto render_finished(
			std::uint32_t image_index
		) const -> gpu::handle<gpu::semaphore>;

	private:
		auto reset() -> void;

		Device* m_device = nullptr;
		std::vector<gpu::handle<gpu::semaphore>> m_image_available;
		std::vector<gpu::handle<gpu::semaphore>> m_render_finished;
	};

	template <sync_device Device>
	class frame_fences final : public non_copyable {
	public:
		frame_fences() = default;
		~frame_fences();

		frame_fences(
			frame_fences&& other
		) noexcept;

		auto operator=(
			frame_fences&& other
		) noexcept -> frame_fences&;

		[[nodiscard]]
		static auto create(
			Device& device,
			std::uint32_t frames_in_flight = max_frames_in_flight
		) -> frame_fences;

		[[nodiscard]]
		auto in_flight(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::fence>;

	private:
		auto reset() -> void;

		Device* m_device = nullptr;
		std::array<std::vector<gpu::handle<gpu::fence>>, gpu::queue_type_count> m_in_flight;
	};
}

template <gse::gpu::sync_device Device>
gse::gpu::swapchain_sync<Device>::~swapchain_sync() {
	reset();
}

template <gse::gpu::sync_device Device>
gse::gpu::swapchain_sync<Device>::swapchain_sync(swapchain_sync&& other) noexcept
	: m_device(other.m_device),
	  m_image_available(std::move(other.m_image_available)),
	  m_render_finished(std::move(other.m_render_finished)) {
	other.m_device = nullptr;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::swapchain_sync<Device>::operator=(swapchain_sync&& other) noexcept -> swapchain_sync& {
	if (this != &other) {
		reset();
		m_device = other.m_device;
		m_image_available = std::move(other.m_image_available);
		m_render_finished = std::move(other.m_render_finished);
		other.m_device = nullptr;
	}
	return *this;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::swapchain_sync<Device>::create(Device& device, const std::uint32_t image_count, const std::uint32_t frames_in_flight) -> swapchain_sync {
	swapchain_sync out;
	out.m_device = &device;
	out.m_image_available.reserve(frames_in_flight);
	for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
		out.m_image_available.push_back(device.create_semaphore());
	}
	out.m_render_finished.reserve(image_count);
	for (std::uint32_t i = 0; i < image_count; ++i) {
		out.m_render_finished.push_back(device.create_semaphore());
	}
	return out;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::swapchain_sync<Device>::image_available(const std::uint32_t frame_index) const -> gpu::handle<gpu::semaphore> {
	return m_image_available[frame_index];
}

template <gse::gpu::sync_device Device>
auto gse::gpu::swapchain_sync<Device>::render_finished(const std::uint32_t image_index) const -> gpu::handle<gpu::semaphore> {
	return m_render_finished[image_index];
}

template <gse::gpu::sync_device Device>
auto gse::gpu::swapchain_sync<Device>::reset() -> void {
	if (!m_device) {
		return;
	}
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
	m_image_available.clear();
	m_render_finished.clear();
	m_device = nullptr;
}

template <gse::gpu::sync_device Device>
gse::gpu::frame_fences<Device>::~frame_fences() {
	reset();
}

template <gse::gpu::sync_device Device>
gse::gpu::frame_fences<Device>::frame_fences(frame_fences&& other) noexcept
	: m_device(other.m_device), m_in_flight(std::move(other.m_in_flight)) {
	other.m_device = nullptr;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::frame_fences<Device>::operator=(frame_fences&& other) noexcept -> frame_fences& {
	if (this != &other) {
		reset();
		m_device = other.m_device;
		m_in_flight = std::move(other.m_in_flight);
		other.m_device = nullptr;
	}
	return *this;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::frame_fences<Device>::create(Device& device, const std::uint32_t frames_in_flight) -> frame_fences {
	frame_fences out;
	out.m_device = &device;
	for (auto& fences : out.m_in_flight) {
		fences.reserve(frames_in_flight);
		for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
			fences.push_back(device.create_fence(true));
		}
	}
	return out;
}

template <gse::gpu::sync_device Device>
auto gse::gpu::frame_fences<Device>::in_flight(const gpu::queue_type queue, const std::uint32_t frame_index) const -> gpu::handle<gpu::fence> {
	return m_in_flight[static_cast<std::size_t>(queue)][frame_index];
}

template <gse::gpu::sync_device Device>
auto gse::gpu::frame_fences<Device>::reset() -> void {
	if (!m_device) {
		return;
	}
	for (const auto& fences : m_in_flight) {
		for (const auto handle : fences) {
			if (handle) {
				m_device->retire(handle);
			}
		}
	}
	for (auto& fences : m_in_flight) {
		fences.clear();
	}
	m_device = nullptr;
}
