export module gse.gpu:vulkan_semaphore;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class semaphore final : public non_copyable {
	public:
		semaphore() = default;

		~semaphore() override = default;

		semaphore(semaphore&&) noexcept = default;

		auto operator=(semaphore&&) noexcept -> semaphore& = default;

		[[nodiscard]] static auto create_binary(const device& dev) -> semaphore;

		[[nodiscard]]
		static auto create_timeline(const device& dev, std::uint64_t initial_value) -> semaphore;

		[[nodiscard]] auto handle(this const semaphore& self) -> gpu::handle<semaphore>;

		explicit operator bool() const;

	private:
		explicit semaphore(vk::raii::Semaphore&& semaphore);

		vk::raii::Semaphore m_semaphore = nullptr;
	};
}

gse::vulkan::semaphore::semaphore(vk::raii::Semaphore&& semaphore) : m_semaphore(std::move(semaphore)) {
}

auto gse::vulkan::semaphore::create_binary(const device& dev) -> semaphore {
	constexpr vk::SemaphoreCreateInfo info{};
	return semaphore(dev.raii_device().createSemaphore(info));
}

auto gse::vulkan::semaphore::create_timeline(const device& dev, const std::uint64_t initial_value) -> semaphore {
	const vk::SemaphoreTypeCreateInfo type_info{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = initial_value,
	};
	const vk::SemaphoreCreateInfo info{
		.pNext = &type_info,
	};
	return semaphore(dev.raii_device().createSemaphore(info));
}

auto gse::vulkan::semaphore::handle(this const semaphore& self) -> gpu::handle<semaphore> {
	return std::bit_cast<gpu::handle<semaphore>>(*self.m_semaphore);
}

gse::vulkan::semaphore::operator bool() const {
	return *m_semaphore != nullptr;
}
