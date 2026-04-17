export module gse.platform:gpu_swapchain;

import std;

import :vulkan_runtime;
import :vulkan_allocator;
import :gpu_device;
import :gpu_image;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	class swap_chain final : public non_copyable {
	public:
		[[nodiscard]] static auto create(
			vec2i framebuffer_size,
			device& dev
		) -> std::unique_ptr<swap_chain>;

		swap_chain(vulkan::swap_chain_config&& config, device& dev);

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		auto clear_depth_image(
		) -> void;

		using recreate_callback = std::function<void()>;
		auto on_recreate(
			recreate_callback callback
		) -> void;

		auto notify_recreated(
		) -> void;

		auto set_config(
			vulkan::swap_chain_config&& config
		) -> void;

		auto recreate(
			vec2i framebuffer_size
		) -> void;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> vk::Image;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> vk::ImageView;

		[[nodiscard]] auto format(
		) const -> vk::Format;

		[[nodiscard]] auto is_bgra(
		) const -> bool;

		[[nodiscard]] auto generation(
		) const -> std::uint64_t;

		[[nodiscard]] auto config(
		) -> vulkan::swap_chain_config&;

		[[nodiscard]] auto config(
		) const -> const vulkan::swap_chain_config&;

	private:
		static auto create_swap_chain_resources(vec2i framebuffer_size, const vulkan::instance_config& instance_data, const vulkan::device_config& device_data, vulkan::allocator& alloc) -> vulkan::swap_chain_config ;

		vulkan::swap_chain_config m_config;
		device* m_device;
		std::vector<recreate_callback> m_recreate_callbacks;
		std::uint64_t m_generation = 0;
	};
}

auto gse::gpu::swap_chain::create(const vec2i framebuffer_size, device& dev) -> std::unique_ptr<swap_chain> {
	auto config = create_swap_chain_resources(framebuffer_size, dev.instance_config(), dev.device_config(), dev.allocator());
	return std::make_unique<swap_chain>(std::move(config), dev);
}

gse::gpu::swap_chain::swap_chain(vulkan::swap_chain_config&& config, device& dev)
	: m_config(std::move(config)), m_device(&dev) {}

auto gse::gpu::swap_chain::extent() const -> vec2u {
	return { m_config.extent.width, m_config.extent.height };
}

auto gse::gpu::swap_chain::depth_image(this auto& self) -> auto& {
	return (self.m_config.depth_image);
}

auto gse::gpu::swap_chain::clear_depth_image() -> void {
	m_config.depth_image = {};
}

auto gse::gpu::swap_chain::on_recreate(recreate_callback callback) -> void {
	m_recreate_callbacks.push_back(std::move(callback));
}

auto gse::gpu::swap_chain::notify_recreated() -> void {
	++m_generation;
	for (const auto& callback : m_recreate_callbacks) {
		callback();
	}
}

auto gse::gpu::swap_chain::set_config(vulkan::swap_chain_config&& config) -> void {
	m_config = std::move(config);
}

auto gse::gpu::swap_chain::recreate(const vec2i framebuffer_size) -> void {
	m_config.swap_chain = nullptr;
	m_config = create_swap_chain_resources(framebuffer_size, m_device->instance_config(), m_device->device_config(), m_device->allocator());
}

auto gse::gpu::swap_chain::image(const std::uint32_t index) const -> vk::Image {
	return m_config.images[index];
}

auto gse::gpu::swap_chain::image_view(const std::uint32_t index) const -> vk::ImageView {
	return *m_config.image_views[index];
}

auto gse::gpu::swap_chain::format() const -> vk::Format {
	return m_config.format;
}

auto gse::gpu::swap_chain::is_bgra() const -> bool {
	return m_config.format == vk::Format::eB8G8R8A8Srgb
		|| m_config.format == vk::Format::eB8G8R8A8Unorm;
}

auto gse::gpu::swap_chain::generation() const -> std::uint64_t {
	return m_generation;
}

auto gse::gpu::swap_chain::config() -> vulkan::swap_chain_config& {
	return m_config;
}

auto gse::gpu::swap_chain::config() const -> const vulkan::swap_chain_config& {
	return m_config;
}

auto gse::gpu::swap_chain::create_swap_chain_resources(const vec2i framebuffer_size, const vulkan::instance_config& instance_data, const vulkan::device_config& device_data, vulkan::allocator& alloc) -> vulkan::swap_chain_config {
	vulkan::swap_chain_details details = {
		device_data.physical_device.getSurfaceCapabilitiesKHR(*instance_data.surface),
		device_data.physical_device.getSurfaceFormatsKHR(*instance_data.surface),
		device_data.physical_device.getSurfacePresentModesKHR(*instance_data.surface)
	};

	vk::SurfaceFormatKHR surface_format;
	for (const auto& available_format : details.formats) {
		if (available_format.format == vk::Format::eB8G8R8A8Srgb &&
			available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surface_format = available_format;
			break;
		}
		surface_format = details.formats[0];
	}

	auto present_mode = vk::PresentModeKHR::eFifo;
	for (const auto& mode : details.present_modes) {
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
			case vk::PresentModeKHR::eImmediate: return "Immediate";
			case vk::PresentModeKHR::eMailbox: return "Mailbox";
			case vk::PresentModeKHR::eFifo: return "FIFO (VSync)";
			case vk::PresentModeKHR::eFifoRelaxed: return "FIFO Relaxed";
			default: return "Unknown";
		}
	};
	log::println(log::category::vulkan, "Present mode: {}", mode_name(present_mode));

	vk::Extent2D extent;
	if (details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = details.capabilities.currentExtent;
	}
	else {
		vk::Extent2D actual_extent = {
			static_cast<uint32_t>(framebuffer_size.x()),
			static_cast<uint32_t>(framebuffer_size.y())
		};

		extent.width = std::clamp(
			actual_extent.width,
			details.capabilities.minImageExtent.width,
			details.capabilities.maxImageExtent.width
		);

		extent.height = std::clamp(
			actual_extent.height,
			details.capabilities.minImageExtent.height,
			details.capabilities.maxImageExtent.height
		);
	}

	std::uint32_t image_count = details.capabilities.minImageCount + 1;
	if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount) {
		image_count = details.capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR create_info{
		.flags = {},
		.surface = *instance_data.surface,
		.minImageCount = image_count,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
	};

	const auto families = vulkan::find_queue_families(device_data.physical_device, instance_data.surface);
	const std::uint32_t queue_family_indices[] = { families.graphics_family.value(), families.present_family.value() };

	if (families.graphics_family != families.present_family) {
		create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else {
		create_info.imageSharingMode = vk::SharingMode::eExclusive;
	}

	create_info.preTransform = details.capabilities.currentTransform;
	create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	create_info.presentMode = present_mode;
	create_info.clipped = true;

	auto swap_chain = device_data.device.createSwapchainKHR(create_info);
	auto images = swap_chain.getImages();
	auto format = surface_format.format;

	auto depth_image = alloc.create_image(
		{
			.flags = {},
			.imageType = vk::ImageType::e2D,
			.format = vk::Format::eD32Sfloat,
			.extent = { extent.width, extent.height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
			.flags = {},
			.image = nullptr,
			.viewType = vk::ImageViewType::e2D,
			.format = vk::Format::eD32Sfloat,
			.components = {},
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eDepth,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		}
	);

	depth_image.current_layout = vk::ImageLayout::eUndefined;
	const vec2u depth_extent{ extent.width, extent.height };
	auto depth_gpu_image = gpu::image(std::move(depth_image), image_format::d32_sfloat, depth_extent);

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
		image_views.emplace_back(device_data.device, iv_create_info);
	}

	return vulkan::swap_chain_config(
		std::move(swap_chain),
		surface_format,
		present_mode,
		extent,
		std::move(images),
		std::move(image_views),
		format,
		std::move(details),
		std::move(depth_gpu_image)
	);
}
