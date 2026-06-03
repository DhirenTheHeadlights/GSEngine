export module gse.vulkan:semaphore;

import std;

import :handles;
import :device;

import gse.core;

export namespace gse::vulkan {
	class semaphore final : public non_copyable {
	public:
		semaphore() {}
		~semaphore();

		semaphore(
			semaphore&& other
		) noexcept;

		auto operator=(
			semaphore&& other
		) noexcept -> semaphore&;

		[[nodiscard]] static auto create_binary(
			device& dev
		) -> semaphore;

		[[nodiscard]]
		static auto create_timeline(
			device& dev,
			std::uint64_t initial_value
		) -> semaphore;

		[[nodiscard]] auto handle() const -> gpu::semaphore_handle;

		[[nodiscard]] auto valid() const -> bool;

	private:
		semaphore(
			device& dev,
			gpu::semaphore_handle handle
		);

		device* m_device = nullptr;
		gpu::semaphore_handle m_semaphore;
	};
}

gse::vulkan::semaphore::semaphore(device& dev, const gpu::semaphore_handle handle)
	: m_device(&dev), m_semaphore(handle) {
}

gse::vulkan::semaphore::~semaphore() {
	if (m_device && m_semaphore) {
		m_device->retire(m_semaphore);
	}
}

gse::vulkan::semaphore::semaphore(semaphore&& other) noexcept
	: m_device(other.m_device), m_semaphore(other.m_semaphore) {
	other.m_device = nullptr;
	other.m_semaphore = {};
}

auto gse::vulkan::semaphore::operator=(semaphore&& other) noexcept -> semaphore& {
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

auto gse::vulkan::semaphore::create_binary(device& dev) -> semaphore {
	return semaphore(dev, dev.create_semaphore());
}

auto gse::vulkan::semaphore::create_timeline(device& dev, const std::uint64_t initial_value) -> semaphore {
	return semaphore(dev, dev.create_timeline_semaphore(initial_value));
}

auto gse::vulkan::semaphore::handle() const -> gpu::semaphore_handle {
	return m_semaphore;
}

auto gse::vulkan::semaphore::valid() const -> bool {
	return static_cast<bool>(m_semaphore);
}
