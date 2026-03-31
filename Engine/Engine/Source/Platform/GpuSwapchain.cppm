export module gse.platform:gpu_swapchain;

import std;

import :vulkan_runtime;
import :vulkan_allocator;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	class swapchain final : public non_copyable {
	public:
		explicit swapchain(vulkan::runtime& runtime);

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto vk_extent(
		) const -> vk::Extent2D;

		[[nodiscard]] auto surface_format(
		) const -> vk::SurfaceFormatKHR;

		[[nodiscard]] auto format(
		) const -> vk::Format;

		[[nodiscard]] auto depth_image(
		) const -> const vulkan::image_resource&;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> vk::ImageView;

		[[nodiscard]] auto images(
		) const -> const std::vector<vk::Image>&;

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

auto gse::gpu::swapchain::vk_extent() const -> vk::Extent2D {
	return m_runtime->swap_chain_config().extent;
}

auto gse::gpu::swapchain::surface_format() const -> vk::SurfaceFormatKHR {
	return m_runtime->swap_chain_config().surface_format;
}

auto gse::gpu::swapchain::format() const -> vk::Format {
	return m_runtime->swap_chain_config().surface_format.format;
}

auto gse::gpu::swapchain::depth_image() const -> const vulkan::image_resource& {
	return m_runtime->swap_chain_config().depth_image;
}

auto gse::gpu::swapchain::image_view(std::uint32_t index) const -> vk::ImageView {
	return *m_runtime->swap_chain_config().image_views[index];
}

auto gse::gpu::swapchain::images() const -> const std::vector<vk::Image>& {
	return m_runtime->swap_chain_config().images;
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
