module gse.vulkan;

import std;
import vulkan;

import gse.log;
import gse.math;

auto gse::vulkan::pick_surface_format(const physical_device& physical_device, const gpu::surface surface) -> gpu::image_format {
	const auto formats = std::bit_cast<vk::PhysicalDevice>(physical_device.handle()).getSurfaceFormatsKHR(std::bit_cast<vk::SurfaceKHR>(surface));
	for (const auto& [format, colorSpace] : formats) {
		if (format == vk::Format::eB8G8R8A8Srgb && colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return from_vk(format);
		}
	}
	return from_vk(formats[0].format);
}

auto gse::vulkan::pick_surface_format(const device& dev, const instance& inst) -> gpu::image_format {
	return pick_surface_format(dev.physical_device(), inst.surface());
}

gse::vulkan::swap_chain_details::swap_chain_details(gpu::surface_capabilities capabilities, std::vector<gpu::surface_format>&& formats, std::vector<gpu::present_mode>&& present_modes)
	: m_capabilities(capabilities), m_formats(std::move(formats)), m_present_modes(std::move(present_modes)) {
}

auto gse::vulkan::swap_chain_details::capabilities() const -> gpu::surface_capabilities {
	return m_capabilities;
}

auto gse::vulkan::swap_chain_details::formats() const -> std::span<const gpu::surface_format> {
	return m_formats;
}

auto gse::vulkan::swap_chain_details::present_modes() const -> std::span<const gpu::present_mode> {
	return m_present_modes;
}

gse::vulkan::swap_chain::swap_chain(vk::raii::SwapchainKHR&& swap_chain, const vk::SurfaceFormatKHR surface_format, const vk::PresentModeKHR present_mode, const vk::Extent2D extent, std::vector<vk::Image>&& images, std::vector<vk::raii::ImageView>&& image_views, const vk::Format format, swap_chain_details&& details, std::vector<fence>&& release_fences)
	: m_swap_chain(std::move(swap_chain)), m_surface_format(surface_format), m_present_mode(present_mode), m_extent(extent), m_images(std::move(images)), m_image_views(std::move(image_views)), m_format(format), m_details(std::move(details)), m_release_fences(std::move(release_fences)) {
}

auto gse::vulkan::swap_chain::create(const vec2i framebuffer_size, const gpu::present_mode preferred_present_mode, const instance& instance_data, device& device_data, const gpu::swap_chain_handle old_swapchain) -> swap_chain {
	const auto vk_surface = std::bit_cast<vk::SurfaceKHR>(instance_data.surface());
	const auto vk_phys = std::bit_cast<vk::PhysicalDevice>(device_data.physical_device().handle());
	const auto vk_capabilities = vk_phys.getSurfaceCapabilitiesKHR(vk_surface);
	auto vk_formats = vk_phys.getSurfaceFormatsKHR(vk_surface);
	auto vk_present_modes = vk_phys.getSurfacePresentModesKHR(vk_surface);

	vk::SurfaceFormatKHR surface_format;
	for (const auto& available_format : vk_formats) {
		if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surface_format = available_format;
			break;
		}
		surface_format = vk_formats[0];
	}

	const auto requested_present_mode = to_vk(preferred_present_mode);
	auto present_mode = vk::PresentModeKHR::eFifo;
	for (const auto& mode : vk_present_modes) {
		if (mode == requested_present_mode) {
			present_mode = mode;
			break;
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
	log::println(
		log::category::vulkan,
		"Present mode: requested {}, granted {}",
		mode_name(requested_present_mode),
		mode_name(present_mode)
	);

	vk::Extent2D extent;
	if (vk_capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
		extent = vk_capabilities.currentExtent;
	}
	else {
		vk::Extent2D actual_extent = { static_cast<std::uint32_t>(framebuffer_size.x()),
									   static_cast<std::uint32_t>(framebuffer_size.y()) };

		extent.width =
			std::clamp(
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

	std::uint32_t image_count = vk_capabilities.minImageCount + 2;
	if (vk_capabilities.maxImageCount > 0 && image_count > vk_capabilities.maxImageCount) {
		image_count = vk_capabilities.maxImageCount;
	}

	log::println(
		log::category::vulkan,
		"Swapchain image count: requested {}, min {}, max {}",
		image_count,
		vk_capabilities.minImageCount,
		vk_capabilities.maxImageCount
	);

	vk::SwapchainCreateInfoKHR create_info{
		.flags = {},
		.surface = vk_surface,
		.minImageCount = image_count,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
	};

	const auto families = find_queue_families(device_data.physical_device(), instance_data.surface());
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

	std::vector<vk::PresentModeKHR> compatible_present_modes;
	{
		const vk::SurfacePresentModeKHR present_mode_query{
			.presentMode = present_mode
		};
		const vk::PhysicalDeviceSurfaceInfo2KHR surface_info{
			.pNext = &present_mode_query,
			.surface = vk_surface,
		};
		vk::SurfacePresentModeCompatibilityKHR compat{};
		vk::SurfaceCapabilities2KHR caps_chain{
			.pNext = &compat
		};
		(void)vk_phys.getSurfaceCapabilities2KHR(&surface_info, &caps_chain);
		compatible_present_modes.resize(compat.presentModeCount);
		compat.pPresentModes = compatible_present_modes.data();
		(void)vk_phys.getSurfaceCapabilities2KHR(&surface_info, &caps_chain);
	}

	const vk::SwapchainPresentModesCreateInfoEXT present_modes_create_info{
		.presentModeCount = static_cast<std::uint32_t>(compatible_present_modes.size()),
		.pPresentModes = compatible_present_modes.data(),
	};
	if (!compatible_present_modes.empty()) {
		create_info.pNext = &present_modes_create_info;
	}

	if (old_swapchain) {
		create_info.oldSwapchain = std::bit_cast<vk::SwapchainKHR>(old_swapchain);
	}

	std::vector<gpu::surface_format> gpu_formats;
	gpu_formats.reserve(vk_formats.size());
	for (const auto& f : vk_formats) {
		gpu_formats.push_back(from_vk(f));
	}

	std::vector<gpu::present_mode> gpu_present_modes;
	gpu_present_modes.reserve(vk_present_modes.size());
	for (const auto m : vk_present_modes) {
		gpu_present_modes.push_back(from_vk(m));
	}

	swap_chain_details details(from_vk(vk_capabilities), std::move(gpu_formats), std::move(gpu_present_modes));

	auto vk_swap_chain = device_data.raii_device().createSwapchainKHR(create_info);
	auto images = vk_swap_chain.getImages();
	auto format = surface_format.format;

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

	std::vector<fence> release_fences;
	release_fences.reserve(images.size());
	for (std::size_t i = 0; i < images.size(); ++i) {
		release_fences.push_back(fence::create(device_data, true));
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
		std::move(release_fences)
	);
}

auto gse::vulkan::swap_chain::handle() const -> gpu::swap_chain_handle {
	return std::bit_cast<gpu::swap_chain_handle>(*m_swap_chain);
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

auto gse::vulkan::swap_chain::image(const std::uint32_t index) const -> gpu::image_handle {
	return std::bit_cast<gpu::image_handle>(m_images[index]);
}

auto gse::vulkan::swap_chain::image_view(const std::uint32_t index) const -> gpu::image_view_handle {
	return std::bit_cast<gpu::image_view_handle>(*m_image_views[index]);
}

auto gse::vulkan::swap_chain::set_present_mode(const gpu::present_mode mode) -> void {
	m_present_mode = to_vk(mode);
}

auto gse::vulkan::swap_chain::release_fence(const std::uint32_t image_index) const -> gpu::fence_handle {
	return m_release_fences[image_index].handle();
}

auto gse::vulkan::swap_chain::wait_release_fences(const device& dev) const -> void {
	for (const auto& f : m_release_fences) {
		(void)dev.wait_for_fence(f.handle());
	}
}

auto gse::vulkan::swap_chain::reset_release_fence(const device& dev, const std::uint32_t image_index) -> void {
	dev.reset_fence(m_release_fences[image_index].handle());
}

auto gse::vulkan::swap_chain::wait_for_present(const std::uint64_t present_id, const std::uint64_t timeout_ns) const -> gpu::result {
	return from_vk(m_swap_chain.waitForPresent(present_id, timeout_ns));
}

auto gse::vulkan::swap_chain::details() const -> const swap_chain_details& {
	return m_details;
}
