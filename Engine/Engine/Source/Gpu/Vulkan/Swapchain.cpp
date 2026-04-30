module gse.gpu;

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
import :vulkan_swapchain;

import gse.log;
import gse.math;

auto gse::vulkan::pick_surface_format(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface) -> gpu::image_format {
	const auto formats = physical_device.getSurfaceFormatsKHR(*surface);
	for (const auto& [format, colorSpace] : formats) {
		if (format == vk::Format::eB8G8R8A8Srgb && colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return from_vk(format);
		}
	}
	return from_vk(formats[0].format);
}

auto gse::vulkan::pick_surface_format(const device& dev, const instance& inst) -> gpu::image_format {
	return pick_surface_format(dev.physical_device(), inst.raii_surface());
}

gse::vulkan::swap_chain_details::swap_chain_details(vk::SurfaceCapabilitiesKHR capabilities, std::vector<vk::SurfaceFormatKHR>&& formats, std::vector<vk::PresentModeKHR>&& present_modes)
	: m_capabilities(std::move(capabilities)),
	  m_formats(std::move(formats)),
	  m_present_modes(std::move(present_modes)) {}

auto gse::vulkan::swap_chain_details::capabilities() const -> gpu::surface_capabilities {
	return from_vk(m_capabilities);
}

auto gse::vulkan::swap_chain_details::formats() const -> std::vector<gpu::surface_format> {
	std::vector<gpu::surface_format> out;
	out.reserve(m_formats.size());
	for (const auto& f : m_formats) {
		out.push_back(from_vk(f));
	}
	return out;
}

auto gse::vulkan::swap_chain_details::present_modes() const -> std::vector<gpu::present_mode> {
	std::vector<gpu::present_mode> out;
	out.reserve(m_present_modes.size());
	for (const auto m : m_present_modes) {
		out.push_back(from_vk(m));
	}
	return out;
}

gse::vulkan::swap_chain::swap_chain(vk::raii::SwapchainKHR&& swap_chain, const vk::SurfaceFormatKHR surface_format, const vk::PresentModeKHR present_mode, const vk::Extent2D extent, std::vector<vk::Image>&& images, std::vector<vk::raii::ImageView>&& image_views, const vk::Format format, swap_chain_details&& details, basic_image<device>&& depth_image)
	: m_swap_chain(std::move(swap_chain)),
	  m_surface_format(surface_format),
	  m_present_mode(present_mode),
	  m_extent(extent),
	  m_images(std::move(images)),
	  m_image_views(std::move(image_views)),
	  m_format(format),
	  m_details(std::move(details)),
	  m_depth_image(std::move(depth_image)) {}

auto gse::vulkan::swap_chain::create(const vec2i framebuffer_size, const instance& instance_data, device& device_data) -> swap_chain {
	const auto vk_capabilities = device_data.physical_device().getSurfaceCapabilitiesKHR(*instance_data.raii_surface());
	auto vk_formats = device_data.physical_device().getSurfaceFormatsKHR(*instance_data.raii_surface());
	auto vk_present_modes = device_data.physical_device().getSurfacePresentModesKHR(*instance_data.raii_surface());

	vk::SurfaceFormatKHR surface_format;
	for (const auto& available_format : vk_formats) {
		if (available_format.format == vk::Format::eB8G8R8A8Srgb &&
			available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surface_format = available_format;
			break;
		}
		surface_format = vk_formats[0];
	}

	auto present_mode = vk::PresentModeKHR::eFifo;
	for (const auto& mode : vk_present_modes) {
		if (mode == vk::PresentModeKHR::eImmediate) {
			present_mode = mode;
			break;
		}
		if (mode == vk::PresentModeKHR::eMailbox && present_mode == vk::PresentModeKHR::eFifo) {
			present_mode = mode;
		}
	}

	auto mode_name = [](vk::PresentModeKHR m) -> std::string_view {
		switch (m) {
			case vk::PresentModeKHR::eImmediate:
				return "Immediate";
			case vk::PresentModeKHR::eMailbox:
				return "Mailbox";
			case vk::PresentModeKHR::eFifo:
				return "FIFO (VSync)";
			case vk::PresentModeKHR::eFifoRelaxed:
				return "FIFO Relaxed";
			default:
				return "Unknown";
		}
	};
	log::println(log::category::vulkan, "Present mode: {}", mode_name(present_mode));

	vk::Extent2D extent;
	if (vk_capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
		extent = vk_capabilities.currentExtent;
	}
	else {
		vk::Extent2D actual_extent = {
			static_cast<std::uint32_t>(framebuffer_size.x()),
			static_cast<std::uint32_t>(framebuffer_size.y())
		};

		extent.width = std::clamp(
			actual_extent.width,
			vk_capabilities.minImageExtent.width,
			vk_capabilities.maxImageExtent.width
		);

		extent.height = std::clamp(
			actual_extent.height,
			vk_capabilities.minImageExtent.height,
			vk_capabilities.maxImageExtent.height
		);
	}

	std::uint32_t image_count = vk_capabilities.minImageCount + 1;
	if (vk_capabilities.maxImageCount > 0 && image_count > vk_capabilities.maxImageCount) {
		image_count = vk_capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR create_info{
		.flags = {},
		.surface = *instance_data.raii_surface(),
		.minImageCount = image_count,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
	};

	const auto families = find_queue_families(device_data.physical_device(), instance_data.raii_surface());
	const std::uint32_t queue_family_indices[] = { families.graphics_family.value(), families.present_family.value() };

	if (families.graphics_family != families.present_family) {
		create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else {
		create_info.imageSharingMode = vk::SharingMode::eExclusive;
	}

	create_info.preTransform = vk_capabilities.currentTransform;
	create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	create_info.presentMode = present_mode;
	create_info.clipped = true;

	swap_chain_details details(vk_capabilities, std::move(vk_formats), std::move(vk_present_modes));

	auto vk_swap_chain = device_data.raii_device().createSwapchainKHR(create_info);
	auto images = vk_swap_chain.getImages();
	auto format = surface_format.format;

	auto depth_image = device_data.create_image(
		gpu::image_create_info{
			.flags = {},
			.type = gpu::image_type::e2d,
			.format = gpu::image_format::d32_sfloat,
			.extent = vec3u{ extent.width, extent.height, 1 },
			.mip_levels = 1,
			.array_layers = 1,
			.samples = gpu::sample_count::e1,
			.usage = gpu::image_flag::depth_attachment | gpu::image_flag::sampled,
		},
		gpu::memory_property_flag::device_local,
		gpu::image_view_create_info{
			.format = gpu::image_format::d32_sfloat,
			.view_type = gpu::image_view_type::e2d,
			.aspects = gpu::image_aspect_flag::depth,
			.base_mip_level = 0,
			.level_count = 1,
			.base_array_layer = 0,
			.layer_count = 1,
		}
	);

	depth_image.set_layout(gpu::image_layout::undefined);

	std::vector<vk::raii::ImageView> image_views;
	image_views.reserve(images.size());

	for (const auto& img : images) {
		vk::ImageViewCreateInfo iv_create_info{
			.flags = {},
			.image = img,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = {},
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		image_views.emplace_back(device_data.raii_device(), iv_create_info);
	}

	return swap_chain(
		std::move(vk_swap_chain),
		surface_format,
		present_mode,
		extent,
		std::move(images),
		std::move(image_views),
		format,
		std::move(details),
		std::move(depth_image)
	);
}

auto gse::vulkan::swap_chain::swap_chain_handle() const -> gpu::handle<swap_chain> {
	return std::bit_cast<gpu::handle<swap_chain>>(*m_swap_chain);
}

auto gse::vulkan::swap_chain::extent() const -> vec2u {
	return { m_extent.width, m_extent.height };
}

auto gse::vulkan::swap_chain::format() const -> gpu::image_format {
	return from_vk(m_format);
}

auto gse::vulkan::swap_chain::surface_format() const -> gpu::surface_format {
	return from_vk(m_surface_format);
}

auto gse::vulkan::swap_chain::present_mode() const -> gpu::present_mode {
	return from_vk(m_present_mode);
}

auto gse::vulkan::swap_chain::image_count() const -> std::uint32_t {
	return static_cast<std::uint32_t>(m_images.size());
}

auto gse::vulkan::swap_chain::image(const std::uint32_t index) const -> gpu::handle<vulkan::image> {
	return std::bit_cast<gpu::handle<vulkan::image>>(m_images[index]);
}

auto gse::vulkan::swap_chain::image_view(const std::uint32_t index) const -> gpu::handle<vulkan::image_view> {
	return std::bit_cast<gpu::handle<vulkan::image_view>>(*m_image_views[index]);
}

auto gse::vulkan::swap_chain::clear_depth() -> void {
	m_depth_image = {};
}

auto gse::vulkan::swap_chain::reset_swapchain() -> void {
	m_swap_chain = nullptr;
}

auto gse::vulkan::swap_chain::details() const -> const swap_chain_details& {
	return m_details;
}
