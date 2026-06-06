export module gse.vulkan:fence;

import std;
import vulkan;

import :handles;
import :device;
import :types;

import gse.core;

export namespace gse::vulkan {
	class fence final : public non_copyable {
	public:
		fence() {}
		~fence() = default;

		fence(
			fence&&
		) noexcept = default;

		auto operator=(
			fence&&
		) noexcept -> fence& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			bool start_signaled
		) -> gpu::expected<fence>;

		[[nodiscard]] auto handle(
			this const fence& self
		) -> gpu::fence_handle;

		[[nodiscard]] auto valid() const -> bool;

	private:
		explicit fence(
			vk::raii::Fence&& fence
		);

		vk::raii::Fence m_fence = nullptr;
	};
}

gse::vulkan::fence::fence(vk::raii::Fence&& fence) : m_fence(std::move(fence)) {
}

auto gse::vulkan::fence::create(const device& dev, const bool start_signaled) -> gpu::expected<fence> {
	const vk::FenceCreateInfo info{
		.flags = start_signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlags{},
	};
	auto [result, vk_fence] = dev.raii_device().createFence(info);
	if (result != vk::Result::eSuccess) {
		return std::unexpected(from_vk(result));
	}
	return fence(std::move(vk_fence));
}

auto gse::vulkan::fence::handle(this const fence& self) -> gpu::fence_handle {
	return std::bit_cast<gpu::fence_handle>(*self.m_fence);
}

auto gse::vulkan::fence::valid() const -> bool {
	return *m_fence != nullptr;
}
