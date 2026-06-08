export module gse.vulkan:queue_timeline;

import std;

import :handles;
import :device;

import gse.core;

export namespace gse::vulkan {
	class queue_timeline final : public non_copyable {
	public:
		queue_timeline() {}
		~queue_timeline();

		queue_timeline(
			queue_timeline&& other
		) noexcept;

		auto operator=(
			queue_timeline&& other
		) noexcept -> queue_timeline&;

		[[nodiscard]] static auto create(
			device& device
		) -> queue_timeline;

		[[nodiscard]] auto handle() const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto read() const -> std::uint64_t;

		auto wait_until(
			std::uint64_t value
		) const -> void;

	private:
		queue_timeline(
			device& device,
			gpu::handle<gpu::semaphore> handle
		);

		device* m_device = nullptr;
		gpu::handle<gpu::semaphore> m_semaphore;
	};
}

gse::vulkan::queue_timeline::queue_timeline(device& device, const gpu::handle<gpu::semaphore> handle)
	: m_device(&device), m_semaphore(handle) {
}

gse::vulkan::queue_timeline::~queue_timeline() {
	if (m_device && m_semaphore) {
		m_device->retire(m_semaphore);
	}
}

gse::vulkan::queue_timeline::queue_timeline(queue_timeline&& other) noexcept
	: m_device(other.m_device), m_semaphore(other.m_semaphore) {
	other.m_device = nullptr;
	other.m_semaphore = {};
}

auto gse::vulkan::queue_timeline::operator=(queue_timeline&& other) noexcept -> queue_timeline& {
	if (this != &other) {
		if (m_device && m_semaphore) {
			m_device->retire(m_semaphore);
		}
		m_device = other.m_device;
		m_semaphore = other.m_semaphore;
		other.m_device = nullptr;
		other.m_semaphore = {};
	}
	return *this;
}

auto gse::vulkan::queue_timeline::create(device& device) -> queue_timeline {
	return queue_timeline(device, device.create_timeline_semaphore(0));
}

auto gse::vulkan::queue_timeline::handle() const -> gpu::handle<gpu::semaphore> {
	return m_semaphore;
}

auto gse::vulkan::queue_timeline::read() const -> std::uint64_t {
	return m_device->semaphore_counter_value(m_semaphore);
}

auto gse::vulkan::queue_timeline::wait_until(const std::uint64_t value) const -> void {
	m_device->wait_semaphore(m_semaphore, value);
}
