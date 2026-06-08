export module gse.gpu_backend:queue_timeline;

import std;

import :core;

import gse.core;

export namespace gse::gpu {
	template <typename T>
	concept timeline_device = requires(T& device, gpu::handle<gpu::semaphore> sem, std::uint64_t value) {
		{ device.create_timeline_semaphore(value) } -> std::same_as<gpu::handle<gpu::semaphore>>;
		{ device.semaphore_counter_value(sem) } -> std::same_as<std::uint64_t>;
		device.wait_semaphore(sem, value);
		device.retire(sem);
	};

	template <timeline_device Device>
	class queue_timeline final : public non_copyable {
	public:
		queue_timeline() = default;
		~queue_timeline();

		queue_timeline(
			queue_timeline&& other
		) noexcept;

		auto operator=(
			queue_timeline&& other
		) noexcept -> queue_timeline&;

		[[nodiscard]] static auto create(
			Device& device,
			std::uint64_t initial_value = 0
		) -> queue_timeline;

		[[nodiscard]] auto handle() const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto read() const -> std::uint64_t;

		auto wait_until(
			std::uint64_t value
		) const -> void;

	private:
		queue_timeline(
			Device& device,
			gpu::handle<gpu::semaphore> semaphore_handle
		);

		auto reset() -> void;

		Device* m_device = nullptr;
		gpu::handle<gpu::semaphore> m_semaphore;
	};
}

template <gse::gpu::timeline_device Device>
gse::gpu::queue_timeline<Device>::~queue_timeline() {
	reset();
}

template <gse::gpu::timeline_device Device>
gse::gpu::queue_timeline<Device>::queue_timeline(queue_timeline&& other) noexcept
	: m_device(other.m_device), m_semaphore(other.m_semaphore) {
	other.m_device = nullptr;
	other.m_semaphore = {};
}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::operator=(queue_timeline&& other) noexcept -> queue_timeline& {
	if (this != &other) {
		reset();
		m_device = other.m_device;
		m_semaphore = other.m_semaphore;
		other.m_device = nullptr;
		other.m_semaphore = {};
	}
	return *this;
}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::create(Device& device, const std::uint64_t initial_value) -> queue_timeline {
	return queue_timeline(device, device.create_timeline_semaphore(initial_value));
}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::handle() const -> gpu::handle<gpu::semaphore> {
	return m_semaphore;
}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::read() const -> std::uint64_t {
	return m_device->semaphore_counter_value(m_semaphore);
}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::wait_until(const std::uint64_t value) const -> void {
	m_device->wait_semaphore(m_semaphore, value);
}

template <gse::gpu::timeline_device Device>
gse::gpu::queue_timeline<Device>::queue_timeline(Device& device, const gpu::handle<gpu::semaphore> semaphore_handle)
	: m_device(&device), m_semaphore(semaphore_handle) {}

template <gse::gpu::timeline_device Device>
auto gse::gpu::queue_timeline<Device>::reset() -> void {
	if (m_device && m_semaphore) {
		m_device->retire(m_semaphore);
	}
}
