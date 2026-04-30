export module gse.gpu:vulkan_fence;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class fence final : public non_copyable {
	public:
		fence() = default;

		~fence() override = default;

		fence(
			fence&&
		) noexcept = default;

		auto operator=(
			fence&&
		) noexcept -> fence& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			bool start_signaled
		) -> fence;

		auto wait(
			const device& dev,
			std::uint64_t timeout_ns
		) const -> bool;

		auto reset(
			const device& dev
		) -> void;

		[[nodiscard]] auto signaled(
			const device& dev
		) const -> bool;

		[[nodiscard]] auto handle(
			this const fence& self
		) -> gpu::handle<fence>;

		explicit operator bool(
		) const;

	private:
		explicit fence(
			vk::raii::Fence&& fence
		);

		vk::raii::Fence m_fence = nullptr;
	};
}

gse::vulkan::fence::fence(vk::raii::Fence&& fence) : m_fence(std::move(fence)) {}

auto gse::vulkan::fence::create(const device& dev, const bool start_signaled) -> fence {
	const vk::FenceCreateInfo info{
		.flags = start_signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlags{},
	};
	return fence(dev.raii_device().createFence(info));
}

auto gse::vulkan::fence::wait(const device& dev, const std::uint64_t timeout_ns) const -> bool {
	const auto result = dev.raii_device().waitForFences(*m_fence, vk::True, timeout_ns);
	return result == vk::Result::eSuccess;
}

auto gse::vulkan::fence::reset(const device& dev) -> void {
	dev.raii_device().resetFences(*m_fence);
}

auto gse::vulkan::fence::signaled(const device& dev) const -> bool {
	return dev.raii_device().getFenceStatus(*m_fence) == vk::Result::eSuccess;
}

auto gse::vulkan::fence::handle(this const fence& self) -> gpu::handle<fence> {
	return std::bit_cast<gpu::handle<fence>>(*self.m_fence);
}

gse::vulkan::fence::operator bool() const {
	return *m_fence != nullptr;
}
