export module gse.platform:gpu_swapchain;

import std;

import :vulkan_runtime;
import :gpu_image;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	class swapchain final : public non_copyable {
	public:
		explicit swapchain(vulkan::runtime& runtime);

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto depth_image(
		) const -> const gpu::image&;

		using recreate_callback = std::function<void()>;
		auto on_recreate(
			recreate_callback callback
		) -> void;

		[[nodiscard]] auto generation(
		) const -> std::uint64_t;

		[[nodiscard]] auto runtime_ref(
		) -> vulkan::runtime&;

	private:
		vulkan::runtime* m_runtime;
	};
}

gse::gpu::swapchain::swapchain(vulkan::runtime& runtime)
	: m_runtime(&runtime) {}

auto gse::gpu::swapchain::extent() const -> vec2u {
	const auto& e = m_runtime->swap_chain_config().extent;
	return { e.width, e.height };
}

auto gse::gpu::swapchain::depth_image() const -> const gpu::image& {
	return m_runtime->swap_chain_config().depth_image;
}

auto gse::gpu::swapchain::on_recreate(recreate_callback callback) -> void {
	m_runtime->on_swap_chain_recreate(std::move(callback));
}

auto gse::gpu::swapchain::generation() const -> std::uint64_t {
	return m_runtime->swap_chain_generation();
}

auto gse::gpu::swapchain::runtime_ref() -> vulkan::runtime& {
	return *m_runtime;
}
