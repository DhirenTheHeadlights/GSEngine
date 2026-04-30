export module gse.gpu:vulkan_swapchain;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_allocation;
import :vulkan_buffer;
import :vulkan_image;
import :vulkan_device;
import :vulkan_instance;
import :vulkan_queues;

import gse.core;
import gse.log;
import gse.math;

export namespace gse::vulkan {
	[[nodiscard]] auto pick_surface_format(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::SurfaceKHR& surface
	) -> gpu::image_format;

	[[nodiscard]] auto pick_surface_format(
		const device& dev,
		const instance& inst
	) -> gpu::image_format;

	class swap_chain_details : public non_copyable {
	public:
		swap_chain_details(
			vk::SurfaceCapabilitiesKHR capabilities,
			std::vector<vk::SurfaceFormatKHR>&& formats,
			std::vector<vk::PresentModeKHR>&& present_modes
		);

		~swap_chain_details(
		) = default;

		swap_chain_details(
			swap_chain_details&&
		) noexcept = default;

		auto operator=(
			swap_chain_details&&
		) noexcept -> swap_chain_details& = default;

		[[nodiscard]] auto capabilities(
		) const -> gpu::surface_capabilities;

		[[nodiscard]] auto formats(
		) const -> std::vector<gpu::surface_format>;

		[[nodiscard]] auto present_modes(
		) const -> std::vector<gpu::present_mode>;

	private:
		vk::SurfaceCapabilitiesKHR m_capabilities;
		std::vector<vk::SurfaceFormatKHR> m_formats;
		std::vector<vk::PresentModeKHR> m_present_modes;
	};

	class swap_chain : public non_copyable {
	public:
		~swap_chain(
		) = default;

		swap_chain(
			swap_chain&&
		) noexcept = default;

		auto operator=(
			swap_chain&&
		) noexcept -> swap_chain& = default;

		[[nodiscard]] static auto create(
			vec2i framebuffer_size,
			const instance& instance_data,
			device& device_data
		) -> swap_chain;

		[[nodiscard]] auto swap_chain_handle(
		) const -> gpu::handle<swap_chain>;

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto format(
		) const -> gpu::image_format;

		[[nodiscard]] auto surface_format(
		) const -> gpu::surface_format;

		[[nodiscard]] auto present_mode(
		) const -> gpu::present_mode;

		[[nodiscard]] auto image_count(
		) const -> std::uint32_t;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> gpu::handle<image>;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> gpu::handle<image_view>;

		[[nodiscard]] auto depth(
			this auto&& self
		) -> auto& {
			return self.m_depth_image;
		}

		auto clear_depth(
		) -> void;

		auto reset_swapchain(
		) -> void;

		[[nodiscard]] auto details(
		) const -> const swap_chain_details&;

	private:
		swap_chain(
			vk::raii::SwapchainKHR&& swap_chain,
			vk::SurfaceFormatKHR surface_format,
			vk::PresentModeKHR present_mode,
			vk::Extent2D extent,
			std::vector<vk::Image>&& images,
			std::vector<vk::raii::ImageView>&& image_views,
			vk::Format format,
			swap_chain_details&& details,
			basic_image<device>&& depth_image
		);

		vk::raii::SwapchainKHR m_swap_chain;
		vk::SurfaceFormatKHR m_surface_format;
		vk::PresentModeKHR m_present_mode;
		vk::Extent2D m_extent;
		std::vector<vk::Image> m_images;
		std::vector<vk::raii::ImageView> m_image_views;
		vk::Format m_format;
		swap_chain_details m_details;
		basic_image<device> m_depth_image;
	};
}
