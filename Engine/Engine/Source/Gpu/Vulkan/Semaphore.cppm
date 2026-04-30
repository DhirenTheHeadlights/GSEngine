export module gse.gpu:vulkan_semaphore;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.assert;
import gse.core;

export namespace gse::gpu {
	enum class semaphore_kind : std::uint8_t {
		binary,
		timeline,
	};
}

export namespace gse::vulkan {
	class semaphore final : public non_copyable {
	public:
		semaphore() = default;

		~semaphore() override = default;

		semaphore(
			semaphore&&
		) noexcept = default;

		auto operator=(
			semaphore&&
		) noexcept -> semaphore& = default;

		[[nodiscard]] static auto create_binary(
			const device& dev
		) -> semaphore;

		[[nodiscard]] static auto create_timeline(
			const device& dev,
			std::uint64_t initial_value
		) -> semaphore;

		auto signal(
			const device& dev,
			std::uint64_t value
		) -> void;

		auto wait(
			const device& dev,
			std::uint64_t value,
			std::uint64_t timeout_ns
		) const -> bool;

		[[nodiscard]] auto value(
			const device& dev
		) const -> std::uint64_t;

		[[nodiscard]] auto kind(
		) const -> gpu::semaphore_kind;

		[[nodiscard]] auto handle(
			this const semaphore& self
		) -> gpu::handle<semaphore>;

		explicit operator bool(
		) const;

	private:
		semaphore(
			vk::raii::Semaphore&& semaphore,
			gpu::semaphore_kind kind
		);

		vk::raii::Semaphore m_semaphore = nullptr;
		gpu::semaphore_kind m_kind = gpu::semaphore_kind::binary;
	};
}

gse::vulkan::semaphore::semaphore(vk::raii::Semaphore&& semaphore, const gpu::semaphore_kind kind)
	: m_semaphore(std::move(semaphore)), m_kind(kind) {}

auto gse::vulkan::semaphore::create_binary(const device& dev) -> semaphore {
	constexpr vk::SemaphoreCreateInfo info{};
	return semaphore(dev.raii_device().createSemaphore(info), gpu::semaphore_kind::binary);
}

auto gse::vulkan::semaphore::create_timeline(const device& dev, const std::uint64_t initial_value) -> semaphore {
	const vk::SemaphoreTypeCreateInfo type_info{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = initial_value,
	};
	const vk::SemaphoreCreateInfo info{
		.pNext = &type_info,
	};
	return semaphore(dev.raii_device().createSemaphore(info), gpu::semaphore_kind::timeline);
}

auto gse::vulkan::semaphore::signal(const device& dev, const std::uint64_t value) -> void {
	assert(m_kind == gpu::semaphore_kind::timeline, std::source_location::current(), "signal() requires a timeline semaphore");
	const vk::SemaphoreSignalInfo info{
		.semaphore = *m_semaphore,
		.value = value,
	};
	dev.raii_device().signalSemaphore(info);
}

auto gse::vulkan::semaphore::wait(const device& dev, const std::uint64_t value, const std::uint64_t timeout_ns) const -> bool {
	assert(m_kind == gpu::semaphore_kind::timeline, std::source_location::current(), "wait() requires a timeline semaphore");
	const vk::Semaphore raw = *m_semaphore;
	const vk::SemaphoreWaitInfo info{
		.semaphoreCount = 1,
		.pSemaphores = &raw,
		.pValues = &value,
	};
	return dev.raii_device().waitSemaphores(info, timeout_ns) == vk::Result::eSuccess;
}

auto gse::vulkan::semaphore::value(const device& dev) const -> std::uint64_t {
	assert(m_kind == gpu::semaphore_kind::timeline, std::source_location::current(), "value() requires a timeline semaphore");
	return dev.raii_device().getSemaphoreCounterValue(*m_semaphore);
}

auto gse::vulkan::semaphore::kind() const -> gpu::semaphore_kind {
	return m_kind;
}

auto gse::vulkan::semaphore::handle(this const semaphore& self) -> gpu::handle<semaphore> {
	return std::bit_cast<gpu::handle<semaphore>>(*self.m_semaphore);
}

gse::vulkan::semaphore::operator bool() const {
	return *m_semaphore != nullptr;
}
