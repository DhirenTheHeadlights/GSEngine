module gse.vulkan:device_impl;

import std;

import :device;
import :aftermath;
import gse.gpu_backend;
import :commands;
import :instance;
import :physical_device;
import :queues;
import :types;
import :sync;
import :shader_object;

import vulkan;

import gse.assert;
import gse.core;
import gse.log;
import gse.math;

namespace gse::vulkan {
	template <typename Feat>
	struct optional_feature {
		Feat features{};
		std::string_view extension_name;
		std::string_view log_yes;
		std::string_view log_no;
		bool supported = false;

		auto attach(void* prev) -> void* {
			features.pNext = prev;
			return supported ? static_cast<void*>(&features) : prev;
		}

		auto register_ext(std::vector<const char*>& exts) const -> void {
			if (supported && !extension_name.empty()) {
				exts.push_back(extension_name.data());
			}
		}

		auto log() const -> void {
			if (supported) {
				if (!log_yes.empty()) {
					log::println(log::category::vulkan, "{}", log_yes);
				}
			}
			else if (!log_no.empty()) {
				log::println(log::level::info, log::category::vulkan, "{}", log_no);
			}
		}
	};

	auto query_descriptor_heap_props(
		const physical_device& pd
	) -> gpu::descriptor_heap_properties;

	auto align_up(
		gpu::device_size value,
		gpu::device_size alignment
	) -> gpu::device_size;
}

auto gse::vulkan::device::host_transition_image_to_general(const gpu::handle<gpu::image> img, const gpu::image_aspect_flag aspect, const std::uint32_t layer_count) const -> void {
	const vk::ImageAspectFlags aspect_mask = aspect == gpu::image_aspect_flag::depth
		? vk::ImageAspectFlagBits::eDepth
		: vk::ImageAspectFlagBits::eColor;
	const vk::HostImageLayoutTransitionInfoEXT transition{
		.image = std::bit_cast<vk::Image>(img),
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eGeneral,
		.subresourceRange = {
			.aspectMask = aspect_mask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = layer_count,
		},
	};
	raii_device().transitionImageLayoutEXT(transition);
}

auto gse::vulkan::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	const auto layer_count = static_cast<std::uint32_t>(layer_pointers.size());
	host_transition_image_to_general(img, gpu::image_aspect_flag::color, layer_count);

	std::vector<vk::MemoryToImageCopyEXT> regions;
	regions.reserve(layer_count);
	for (std::uint32_t i = 0; i < layer_count; ++i) {
		regions.push_back({
			.pHostPointer = layer_pointers[i],
			.memoryRowLength = 0,
			.memoryImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = i,
				.layerCount = 1,
			},
			.imageOffset = { 0, 0, 0 },
			.imageExtent = { extent.x(), extent.y(), 1 },
		});
	}

	const vk::CopyMemoryToImageInfoEXT info{
		.flags = {},
		.dstImage = std::bit_cast<vk::Image>(img),
		.dstImageLayout = vk::ImageLayout::eGeneral,
		.regionCount = static_cast<std::uint32_t>(regions.size()),
		.pRegions = regions.data(),
	};
	raii_device().copyMemoryToImageEXT(info);
}

auto gse::vulkan::device::wait_for_fence(const gpu::handle<gpu::fence> fence, const std::uint64_t timeout_ns) const -> gpu::result {
	const auto vk_fence = std::bit_cast<vk::Fence>(fence);
	const auto vk_result = raii_device().waitForFences(vk_fence, vk::True, timeout_ns);
	return from_vk(vk_result);
}

auto gse::vulkan::device::reset_fence(const gpu::handle<gpu::fence> fence) const -> void {
	raii_device().resetFences(std::bit_cast<vk::Fence>(fence));
}

auto gse::vulkan::device::acquire_next_image(const gpu::swap_chain_handle swapchain, const gpu::handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	const vk::AcquireNextImageInfoKHR info{
		.swapchain = std::bit_cast<vk::SwapchainKHR>(swapchain),
		.timeout = timeout_ns,
		.semaphore = std::bit_cast<vk::Semaphore>(wait_semaphore),
		.fence = nullptr,
		.deviceMask = 1,
	};
	auto [vk_result, image_index] = raii_device().acquireNextImage2KHR(info);
	return {
		.result = from_vk(vk_result),
		.image_index = image_index,
	};
}

auto gse::vulkan::device::create_swap_chain(const vec2i framebuffer_size, const gpu::present_mode preferred_present_mode, const gpu::swap_chain_handle old_swapchain) -> gpu::swap_chain_info {
	const auto vk_surface = std::bit_cast<vk::SurfaceKHR>(m_surface);
	const auto vk_phys = std::bit_cast<vk::PhysicalDevice>(m_physical_device.handle());
	auto [caps_result, vk_capabilities] = vk_phys.getSurfaceCapabilitiesKHR(vk_surface);
	assert(caps_result == vk::Result::eSuccess, "failed to query surface capabilities: {}", vk::to_string(caps_result));
	auto [formats_result, vk_formats] = vk_phys.getSurfaceFormatsKHR(vk_surface);
	assert(formats_result == vk::Result::eSuccess, "failed to query surface formats: {}", vk::to_string(formats_result));
	auto [modes_result, vk_present_modes] = vk_phys.getSurfacePresentModesKHR(vk_surface);
	assert(modes_result == vk::Result::eSuccess, "failed to query surface present modes: {}", vk::to_string(modes_result));

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
		.flags = vk::SwapchainCreateFlagBitsKHR::ePresentTimingEXT | vk::SwapchainCreateFlagBitsKHR::ePresentId2,
		.surface = vk_surface,
		.minImageCount = image_count,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
	};

	const auto families = find_queue_families(m_physical_device, m_surface);
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

	auto [swapchain_result, vk_swap_chain] = raii_device().createSwapchainKHR(create_info);
	assert(swapchain_result == vk::Result::eSuccess, "failed to create swapchain: {}", vk::to_string(swapchain_result));
	auto [images_result, images] = vk_swap_chain.getImages();
	assert(images_result == vk::Result::eSuccess, "failed to get swapchain images: {}", vk::to_string(images_result));
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
		auto [view_result, view] = raii_device().createImageView(iv_create_info);
		assert(view_result == vk::Result::eSuccess, "failed to create swapchain image view: {}", vk::to_string(view_result));
		image_views.push_back(std::move(view));
	}

	std::vector<vk::raii::Fence> release_fences;
	release_fences.reserve(images.size());
	for (std::size_t i = 0; i < images.size(); ++i) {
		auto [fence_result, fence] = raii_device().createFence(vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
		assert(fence_result == vk::Result::eSuccess, "failed to create release fence: {}", vk::to_string(fence_result));
		release_fences.push_back(std::move(fence));
	}

	const auto vk_device = *raii_device();
	const auto vk_swapchain = *vk_swap_chain;

	bool present_timing_supported = false;
	{
		const vk::SurfacePresentModeKHR timing_present_mode{
			.presentMode = present_mode,
		};
		const vk::PhysicalDeviceSurfaceInfo2KHR timing_surface_info{
			.pNext = &timing_present_mode,
			.surface = vk_surface,
		};
		vk::PresentTimingSurfaceCapabilitiesEXT timing_caps{};
		vk::SurfaceCapabilities2KHR timing_caps_chain{
			.pNext = &timing_caps,
		};
		(void)vk_phys.getSurfaceCapabilities2KHR(&timing_surface_info, &timing_caps_chain);
		present_timing_supported = timing_caps.presentTimingSupported == vk::True;
	}
	assert(present_timing_supported, "VK_EXT_present_timing is not supported on this surface");

	const auto timing_queue_result = vk_device.setSwapchainPresentTimingQueueSizeEXT(vk_swapchain, 32);
	assert(
		timing_queue_result == vk::Result::eSuccess || timing_queue_result == vk::Result::eNotReady,
		"failed to set swapchain present timing queue size: {}",
		vk::to_string(timing_queue_result)
	);

	time_t<std::uint64_t> refresh_interval{};
	time_t<std::uint64_t> refresh_duration{};
	{
		const auto [props_result, props] = vk_device.getSwapchainTimingPropertiesEXT(vk_swapchain);
		if (props_result == vk::Result::eSuccess) {
			refresh_interval = time_t<std::uint64_t>(props.first.refreshInterval);
			refresh_duration = time_t<std::uint64_t>(props.first.refreshDuration);
		}
	}

	std::uint64_t time_domain_id = 0;
	{
		vk::SwapchainTimeDomainPropertiesEXT domain_props{};
		std::uint64_t domains_counter = 0;
		(void)vk_device.getSwapchainTimeDomainPropertiesEXT(vk_swapchain, &domain_props, &domains_counter);
		std::vector<vk::TimeDomainKHR> domains(domain_props.timeDomainCount);
		std::vector<std::uint64_t> domain_ids(domain_props.timeDomainCount);
		domain_props.pTimeDomains = domains.data();
		domain_props.pTimeDomainIds = domain_ids.data();
		(void)vk_device.getSwapchainTimeDomainPropertiesEXT(vk_swapchain, &domain_props, &domains_counter);
		for (std::uint32_t i = 0; i < domain_props.timeDomainCount; ++i) {
			if (domains[i] == vk::TimeDomainKHR::eSwapchainLocalEXT) {
				time_domain_id = domain_ids[i];
			}
		}
		if (time_domain_id == 0 && !domain_ids.empty()) {
			time_domain_id = domain_ids[0];
		}
	}

	const auto handle = std::bit_cast<gpu::swap_chain_handle>(*vk_swap_chain);

	gpu::swap_chain_info info{
		.handle = handle,
		.extent = { extent.width, extent.height },
		.format = from_vk(format),
		.present_mode = from_vk(present_mode),
		.present_timing_supported = present_timing_supported,
		.refresh_interval = refresh_interval,
		.refresh_duration = refresh_duration,
		.time_domain_id = time_domain_id,
	};
	info.images.reserve(images.size());
	for (const auto& img : images) {
		info.images.push_back(std::bit_cast<gpu::handle<gpu::image>>(img));
	}
	info.image_views.reserve(image_views.size());
	for (const auto& view : image_views) {
		info.image_views.push_back(std::bit_cast<gpu::handle<gpu::image_view>>(*view));
	}

	{
		std::lock_guard lock(m_mutex);
		m_owned.store(
			handle.value,
			swap_chain_resources{
				.swapchain = std::move(vk_swap_chain),
				.image_views = std::move(image_views),
				.release_fences = std::move(release_fences),
			}
		);
		if (old_swapchain) {
			m_owned.retire(old_swapchain, m_resource_frame + gpu::max_frames_in_flight);
		}
	}

	return info;
}

auto gse::vulkan::device::wait_swapchain_release_fences(const gpu::swap_chain_handle swapchain) const -> void {
	const auto* resources = m_owned.find(swapchain);
	if (!resources) {
		return;
	}
	for (const auto& release_fence : resources->release_fences) {
		(void)wait_for_fence(std::bit_cast<gpu::handle<gpu::fence>>(*release_fence));
	}
}

auto gse::vulkan::device::reset_swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> void {
	const auto* resources = m_owned.find(swapchain);
	if (!resources) {
		return;
	}
	reset_fence(std::bit_cast<gpu::handle<gpu::fence>>(*resources->release_fences[image_index]));
}

auto gse::vulkan::device::swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> gpu::handle<gpu::fence> {
	const auto* resources = m_owned.find(swapchain);
	if (!resources) {
		return {};
	}
	return std::bit_cast<gpu::handle<gpu::fence>>(*resources->release_fences[image_index]);
}

auto gse::vulkan::device::swapchain_past_presentation_timing(const gpu::swap_chain_handle swapchain) const -> std::vector<gpu::past_present_timing> {
	const auto* resources = m_owned.find(swapchain);
	if (!resources) {
		return {};
	}

	const auto vk_device = *raii_device();
	const auto vk_swapchain = *resources->swapchain;
	const vk::PastPresentationTimingInfoEXT info{
		.swapchain = vk_swapchain,
	};

	constexpr std::uint32_t batch = 16;
	constexpr std::uint32_t max_stages = 4;

	std::vector<gpu::past_present_timing> out;
	for (;;) {
		std::array<vk::PastPresentationTimingEXT, batch> timings{};
		std::array<vk::PresentStageTimeEXT, batch * max_stages> stages{};
		for (std::uint32_t i = 0; i < batch; ++i) {
			timings[i].presentStageCount = max_stages;
			timings[i].pPresentStages = stages.data() + static_cast<std::size_t>(i) * max_stages;
		}
		vk::PastPresentationTimingPropertiesEXT props{
			.presentationTimingCount = batch,
			.pPresentationTimings = timings.data(),
		};
		const auto result = vk_device.getPastPresentationTimingEXT(&info, &props);
		if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) {
			break;
		}
		const auto got = props.presentationTimingCount;
		for (std::uint32_t i = 0; i < got; ++i) {
			const auto& timing = timings[i];
			gpu::past_present_timing sample{
				.present_id = timing.presentId,
				.complete = timing.reportComplete == vk::True,
			};
			for (std::uint32_t s = 0; s < timing.presentStageCount; ++s) {
				if (timing.pPresentStages[s].stage & vk::PresentStageFlagBitsEXT::eImageFirstPixelOut) {
					sample.first_pixel_out = time_t<std::uint64_t>(timing.pPresentStages[s].time);
				}
			}
			out.push_back(sample);
		}
		if (got == 0 || result == vk::Result::eSuccess) {
			break;
		}
	}
	return out;
}

auto gse::vulkan::pick_surface_format(const physical_device& physical_device, const gpu::surface surface) -> gpu::image_format {
	auto [formats_result, formats] = std::bit_cast<vk::PhysicalDevice>(physical_device.handle()).getSurfaceFormatsKHR(std::bit_cast<vk::SurfaceKHR>(surface));
	assert(formats_result == vk::Result::eSuccess, "failed to query surface formats: {}", vk::to_string(formats_result));
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

gse::vulkan::device::~device() {
	clean_up();
}

gse::vulkan::device::device(device&& other) noexcept
	: m_physical_device(std::move(other.m_physical_device)), m_device(std::move(other.m_device)), m_fault_enabled(other.m_fault_enabled), m_vendor_binary_fault_enabled(other.m_vendor_binary_fault_enabled), m_queue_families(other.m_queue_families), m_surface(other.m_surface), m_pools(std::move(other.m_pools)), m_live_allocation_count(other.m_live_allocation_count.load()), m_next_allocation_id(other.m_next_allocation_id.load()), m_cleaned_up(other.m_cleaned_up), m_settings(other.m_settings), m_live_allocations(std::move(other.m_live_allocations)) {
}

auto gse::vulkan::device::operator=(device&& other) noexcept -> device& {
	if (this != &other) {
		clean_up();
		m_physical_device = std::move(other.m_physical_device);
		m_device = std::move(other.m_device);
		m_fault_enabled = other.m_fault_enabled;
		m_vendor_binary_fault_enabled = other.m_vendor_binary_fault_enabled;
		m_queue_families = other.m_queue_families;
		m_surface = other.m_surface;
		m_pools = std::move(other.m_pools);
		m_live_allocation_count = other.m_live_allocation_count.load();
		m_next_allocation_id = other.m_next_allocation_id.load();
		m_cleaned_up = other.m_cleaned_up;
		m_settings = other.m_settings;
		m_live_allocations = std::move(other.m_live_allocations);
	}
	return *this;
}

auto gse::vulkan::device::queue_family(const gpu::queue_type queue) const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(queue)];
}

auto gse::vulkan::device::graphics_family() const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)];
}

auto gse::vulkan::device::compute_family() const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(gpu::queue_type::compute)];
}

auto gse::vulkan::device::families_distinct() const -> bool {
	for (std::size_t i = 1; i < gpu::queue_type_count; ++i) {
		if (m_queue_families[i] != m_queue_families[0]) {
			return true;
		}
	}
	return false;
}

auto gse::vulkan::device::create(const instance& instance_data, gpu::device_settings& cfg, aftermath& aftermath_tracker) -> device_creation_result {
	auto devices = instance_data.enumerate_physical_devices();
	assert(!devices.empty(), "No Vulkan-compatible GPUs found!");

	std::size_t selected_index = 0;
	for (std::size_t i = 0; i < devices.size(); ++i) {
		if (devices[i].is_discrete()) {
			selected_index = i;
			break;
		}
	}

	class physical_device physical_device = std::move(devices[selected_index]);
	const auto vk_physical_device = std::bit_cast<vk::PhysicalDevice>(physical_device.handle());

	const auto physical_device_properties = vk_physical_device.getProperties();
	log::println(
		log::category::vulkan,
		"Selected GPU: {}",
		std::string_view(physical_device_properties.deviceName.data())
	);

	const auto families = find_queue_families(physical_device, instance_data.surface());

	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	std::set unique_queue_families = {
		families.graphics_family.value(),
		families.present_family.value(),
		families.compute_family.value(),
	};

	if (families.video_encode_family.has_value()) {
		unique_queue_families.insert(families.video_encode_family.value());
	}

	float queue_priority = 1.0f;
	for (std::uint32_t queue_family_index : unique_queue_families) {
		vk::DeviceQueueCreateInfo queue_create_info{
			.flags = {},
			.queueFamilyIndex = queue_family_index,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority,
		};
		queue_create_infos.push_back(queue_create_info);
	}

	auto [ext_result, available_extensions] = vk_physical_device.enumerateDeviceExtensionProperties();
	assert(ext_result == vk::Result::eSuccess, "failed to enumerate device extensions: {}", vk::to_string(ext_result));
	const auto supports_extension = [&](const char* extension_name) {
		return std::ranges::any_of(
			available_extensions,
			[&](const auto& extension) {
				return std::strcmp(extension.extensionName.data(), extension_name) == 0;
			}
		);
	};

	assert(supports_extension(vk::EXTShaderObjectExtensionName), "VK_EXT_shader_object is required");
	assert(supports_extension(vk::EXTExtendedDynamicState3ExtensionName), "VK_EXT_extended_dynamic_state3 is required");
	assert(
		supports_extension(vk::EXTVertexInputDynamicStateExtensionName),
		"VK_EXT_vertex_input_dynamic_state is required"
	);
	assert(supports_extension(vk::EXTDescriptorHeapExtensionName), "VK_EXT_descriptor_heap is required");
	assert(
		supports_extension(vk::KHRDeferredHostOperationsExtensionName) && supports_extension(vk::KHRAccelerationStructureExtensionName) && supports_extension(vk::KHRRayQueryExtensionName),
		"Ray tracing extensions are required"
	);

	const bool video_encode_extensions_available =
		families.video_encode_family.has_value() &&
		supports_extension(vk::KHRVideoQueueExtensionName) &&
		supports_extension(vk::KHRVideoEncodeQueueExtensionName);

	const auto feature_chain = vk_physical_device.getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceMeshShaderFeaturesEXT,
		vk::PhysicalDeviceFaultFeaturesEXT,
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
		vk::PhysicalDeviceRayQueryFeaturesKHR,
		vk::PhysicalDeviceRobustness2FeaturesEXT,
		vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR,
		vk::PhysicalDeviceHostImageCopyFeaturesEXT,
		vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT,
		vk::PhysicalDevicePresentTimingFeaturesEXT,
		vk::PhysicalDevicePresentId2FeaturesKHR,
		vk::PhysicalDeviceDescriptorHeapFeaturesEXT,
		vk::PhysicalDeviceShaderObjectFeaturesEXT,
		vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT,
		vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT
	>();

	const auto& vk12_query = feature_chain.get<vk::PhysicalDeviceVulkan12Features>();
	const auto& mesh_shader_query = feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
	const auto& as_query = feature_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
	const auto& rq_query = feature_chain.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
	const auto& fault_query = feature_chain.get<vk::PhysicalDeviceFaultFeaturesEXT>();
	const auto& robustness2_query = feature_chain.get<vk::PhysicalDeviceRobustness2FeaturesEXT>();
	const auto& unified_layouts_query = feature_chain.get<vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>();
	const auto& host_image_copy_query = feature_chain.get<vk::PhysicalDeviceHostImageCopyFeaturesEXT>();
	const auto& swapchain_maintenance1_query = feature_chain.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>();
	const auto& present_timing_query = feature_chain.get<vk::PhysicalDevicePresentTimingFeaturesEXT>();
	const auto& present_id2_query = feature_chain.get<vk::PhysicalDevicePresentId2FeaturesKHR>();
	const auto& descriptor_heap_query = feature_chain.get<vk::PhysicalDeviceDescriptorHeapFeaturesEXT>();
	const auto& shader_object_query = feature_chain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>();
	const auto& eds3_query = feature_chain.get<vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT>();
	const auto& vertex_input_dynamic_state_query = feature_chain.get<vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT>();

	assert(mesh_shader_query.meshShader && mesh_shader_query.taskShader, "Mesh shaders required");
	assert(as_query.accelerationStructure && rq_query.rayQuery, "Ray tracing features required");
	assert(descriptor_heap_query.descriptorHeap, "VK_EXT_descriptor_heap feature required");
	assert(shader_object_query.shaderObject, "VK_EXT_shader_object feature required");
	assert(vertex_input_dynamic_state_query.vertexInputDynamicState, "vertexInputDynamicState required");

	const bool device_fault_supported = supports_extension(vk::EXTDeviceFaultExtensionName) && fault_query.deviceFault;
	const bool device_fault_vendor_binary_supported = device_fault_supported && fault_query.deviceFaultVendorBinary;
	const bool av1_encode_supported = video_encode_extensions_available && supports_extension(vk::KHRVideoEncodeAv1ExtensionName);

	assert(
		supports_extension(vk::EXTRobustness2ExtensionName) && robustness2_query.robustBufferAccess2,
		"VK_EXT_robustness2 is required"
	);
	assert(
		supports_extension(vk::KHRUnifiedImageLayoutsExtensionName) && unified_layouts_query.unifiedImageLayouts,
		"VK_KHR_unified_image_layouts is required"
	);
	assert(
		supports_extension(vk::EXTHostImageCopyExtensionName) && host_image_copy_query.hostImageCopy,
		"VK_EXT_host_image_copy is required"
	);
	assert(
		supports_extension(vk::EXTSwapchainMaintenance1ExtensionName) && swapchain_maintenance1_query.swapchainMaintenance1,
		"VK_EXT_swapchain_maintenance1 is required"
	);
	assert(
		supports_extension(vk::EXTPresentTimingExtensionName) && present_timing_query.presentTiming && present_timing_query.presentAtRelativeTime,
		"VK_EXT_present_timing with relative-time scheduling is required"
	);
	assert(
		supports_extension(vk::KHRPresentId2ExtensionName) && present_id2_query.presentId2,
		"VK_KHR_present_id2 is required"
	);
	assert(
		supports_extension(vk::KHRCalibratedTimestampsExtensionName),
		"VK_KHR_calibrated_timestamps is required"
	);

	vk::PhysicalDeviceMemoryPriorityFeaturesEXT memory_priority_features{
		.memoryPriority = vk::True,
	};
	vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_memory_features{
		.pNext = &memory_priority_features,
		.pageableDeviceLocalMemory = vk::True,
	};
	vk::PhysicalDeviceMaintenance6FeaturesKHR maintenance6_features{
		.pNext = &pageable_memory_features,
		.maintenance6 = vk::True,
	};
	vk::PhysicalDeviceMaintenance5FeaturesKHR maintenance5_features{
		.pNext = &maintenance6_features,
		.maintenance5 = vk::True,
	};
	vk::PhysicalDeviceVulkan13Features vulkan13_features{
		.pNext = &maintenance5_features,
		.subgroupSizeControl = vk::True,
		.computeFullSubgroups = vk::True,
		.synchronization2 = vk::True,
		.dynamicRendering = vk::True,
	};
	vk::PhysicalDeviceVulkan12Features vulkan12_features{
		.pNext = &vulkan13_features,
		.storageBuffer8BitAccess = vk::True,
		.uniformAndStorageBuffer8BitAccess = vk::True,
		.shaderInt8 = vk::True,
		.shaderSampledImageArrayNonUniformIndexing = vk::True,
		.descriptorBindingSampledImageUpdateAfterBind = vk::True,
		.descriptorBindingStorageImageUpdateAfterBind = vk::True,
		.descriptorBindingStorageBufferUpdateAfterBind = vk::True,
		.descriptorBindingPartiallyBound = vk::True,
		.descriptorBindingVariableDescriptorCount = vk::True,
		.runtimeDescriptorArray = vk::True,
		.scalarBlockLayout = vk::True,
		.hostQueryReset = vk::True,
		.timelineSemaphore = vk::True,
		.bufferDeviceAddress = vk::True,
		.bufferDeviceAddressCaptureReplay = vk12_query.bufferDeviceAddressCaptureReplay,
	};
	vk::PhysicalDeviceVulkan11Features vulkan11_features{
		.pNext = &vulkan12_features,
		.storageBuffer16BitAccess = vk::True,
		.uniformAndStorageBuffer16BitAccess = vk::True,
		.shaderDrawParameters = vk::True,
	};
	vk::PhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
		.pNext = &vulkan11_features,
		.taskShader = vk::True,
		.meshShader = vk::True,
	};
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features{
		.pNext = &mesh_shader_features,
		.accelerationStructure = vk::True,
		.accelerationStructureCaptureReplay = as_query.accelerationStructureCaptureReplay,
	};
	vk::PhysicalDeviceRayQueryFeaturesKHR ray_query_features{
		.pNext = &as_features,
		.rayQuery = vk::True,
	};
	vk::PhysicalDeviceDescriptorHeapFeaturesEXT descriptor_heap_features{
		.pNext = &ray_query_features,
		.descriptorHeap = vk::True,
	};
	vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_features{
		.pNext = &descriptor_heap_features,
		.shaderObject = vk::True,
	};
	auto eds3_features = eds3_query;
	eds3_features.pNext = &shader_object_features;
	vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{
		.pNext = &eds3_features,
		.vertexInputDynamicState = vk::True,
	};
	vk::PhysicalDeviceRobustness2FeaturesEXT robustness2_features{
		.pNext = &vertex_input_dynamic_state_features,
		.robustBufferAccess2 = vk::True,
		.robustImageAccess2 = vk::True,
		.nullDescriptor = vk::True,
	};
	vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR unified_layouts_features{
		.pNext = &robustness2_features,
		.unifiedImageLayouts = vk::True,
	};
	vk::PhysicalDeviceHostImageCopyFeaturesEXT host_image_copy_features{
		.pNext = &unified_layouts_features,
		.hostImageCopy = vk::True,
	};
	vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1_features{
		.pNext = &host_image_copy_features,
		.swapchainMaintenance1 = vk::True,
	};
	vk::PhysicalDevicePresentId2FeaturesKHR present_id2_features{
		.pNext = &swapchain_maintenance1_features,
		.presentId2 = vk::True,
	};
	vk::PhysicalDevicePresentTimingFeaturesEXT present_timing_features{
		.pNext = &present_id2_features,
		.presentTiming = vk::True,
		.presentAtRelativeTime = vk::True,
	};

	optional_feature<vk::PhysicalDeviceFaultFeaturesEXT> fault{
		.features = {
			.deviceFault = device_fault_supported,
			.deviceFaultVendorBinary = device_fault_vendor_binary_supported,
		},
		.extension_name = vk::EXTDeviceFaultExtensionName,
		.log_yes = "Device fault support detected",
		.supported = device_fault_supported,
	};
	optional_feature<vk::PhysicalDeviceVideoEncodeAV1FeaturesKHR> av1_encode{
		.features = {
			.videoEncodeAV1 = vk::True
		},
		.supported = av1_encode_supported,
	};

	std::vector device_extensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRDynamicRenderingExtensionName,
		vk::KHRMaintenance5ExtensionName,
		vk::KHRMaintenance6ExtensionName,
		vk::KHRCalibratedTimestampsExtensionName,
		vk::EXTPageableDeviceLocalMemoryExtensionName,
		vk::EXTMemoryPriorityExtensionName,
		vk::EXTMeshShaderExtensionName,
		vk::KHRDeferredHostOperationsExtensionName,
		vk::KHRAccelerationStructureExtensionName,
		vk::KHRRayQueryExtensionName,
		vk::EXTShaderObjectExtensionName,
		vk::EXTExtendedDynamicState3ExtensionName,
		vk::EXTVertexInputDynamicStateExtensionName,
		vk::EXTDescriptorHeapExtensionName,
		vk::EXTRobustness2ExtensionName,
		vk::KHRUnifiedImageLayoutsExtensionName,
		vk::EXTHostImageCopyExtensionName,
		vk::EXTSwapchainMaintenance1ExtensionName,
		vk::KHRPresentId2ExtensionName,
		vk::EXTPresentTimingExtensionName,
	};

	void* chain_head = &present_timing_features;
	auto process = [&](auto&... features) {
		((chain_head = features.attach(chain_head)), ...);
		(features.register_ext(device_extensions), ...);
		(features.log(), ...);
	};
	process(fault, av1_encode);

	for (const auto* aftermath_ext : aftermath_tracker.required_device_extensions()) {
		if (supports_extension(aftermath_ext)) {
			device_extensions.push_back(aftermath_ext);
		}
	}

	if (video_encode_extensions_available) {
		device_extensions.push_back(vk::KHRVideoQueueExtensionName);
		device_extensions.push_back(vk::KHRVideoEncodeQueueExtensionName);
		if (av1_encode_supported) {
			device_extensions.push_back(vk::KHRVideoEncodeAv1ExtensionName);
		}
		if (supports_extension(vk::KHRVideoEncodeH265ExtensionName)) {
			device_extensions.push_back(vk::KHRVideoEncodeH265ExtensionName);
		}
	}

	chain_head = aftermath_tracker.device_create_info_pnext(chain_head);

	vk::PhysicalDeviceFeatures2 features2{
		.pNext = chain_head,
		.features = {
			.robustBufferAccess = vk::True,
			.drawIndirectFirstInstance = vk::True,
			.fillModeNonSolid = vk::True,
			.samplerAnisotropy = vk::True,
			.pipelineStatisticsQuery = vk::True,
			.shaderInt64 = vk::True,
		},
	};

	vk::DeviceCreateInfo create_info{
		.pNext = &features2,
		.flags = {},
		.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
		.pQueueCreateInfos = queue_create_infos.data(),
		.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
		.ppEnabledExtensionNames = device_extensions.data(),
	};

	vk::raii::Device logical_device = physical_device.create_device(create_info);

	vk::detail::defaultDispatchLoaderDynamic.init(*logical_device);

	log::println(log::category::vulkan, "Logical Device Created Successfully!");

	const auto graphics_queue = std::bit_cast<gpu::handle<gpu::queue>>(*logical_device.getQueue(families.graphics_family.value(), 0));
	const auto present_queue = std::bit_cast<gpu::handle<gpu::queue>>(*logical_device.getQueue(families.present_family.value(), 0));
	const auto compute_queue = std::bit_cast<gpu::handle<gpu::queue>>(*logical_device.getQueue(families.compute_family.value(), 0));

	gpu::handle<gpu::queue> video_encode_queue;
	std::optional<std::uint32_t> video_encode_family;
	if (video_encode_extensions_available) {
		video_encode_queue = std::bit_cast<gpu::handle<gpu::queue>>(*logical_device.getQueue(families.video_encode_family.value(), 0));
		video_encode_family = families.video_encode_family.value();
		log::println(
			log::category::vulkan,
			"Video encode queue acquired (family {})",
			families.video_encode_family.value()
		);
	}

	return device_creation_result{
		.device = device(
			std::move(physical_device),
			std::move(logical_device),
			cfg,
			device_fault_supported,
			device_fault_vendor_binary_supported,
			families.graphics_family.value(),
			families.compute_family.value(),
			instance_data.surface()
		),
		.queue = queue(
			graphics_queue,
			present_queue,
			compute_queue,
			families.graphics_family.value(),
			families.compute_family.value(),
			video_encode_queue,
			video_encode_family
		),
		.families = families,
		.video_encode_enabled = video_encode_extensions_available,
	};
}

auto gse::vulkan::device::device_handle() const -> gpu::device_handle {
	return std::bit_cast<gpu::device_handle>(*m_device);
}

auto gse::vulkan::device::fault_enabled() const -> bool {
	return m_fault_enabled;
}

auto gse::vulkan::device::vendor_binary_fault_enabled() const -> bool {
	return m_vendor_binary_fault_enabled;
}

auto gse::vulkan::device::wait_idle() const -> void {
	m_device.waitIdle();
}

auto gse::vulkan::device::timestamp_period() const -> float {
	return m_physical_device.timestamp_period();
}

auto gse::vulkan::device::query_fault_counts(gpu::device_fault_counts& counts) const -> gpu::result {
	vk::DeviceFaultCountsEXT vk_counts{};
	const auto vk_result = m_device.getFaultInfoEXT(&vk_counts, nullptr);
	counts.address_info_count = vk_counts.addressInfoCount;
	counts.vendor_info_count = vk_counts.vendorInfoCount;
	counts.vendor_binary_size = vk_counts.vendorBinarySize;
	return from_vk(vk_result);
}

auto gse::vulkan::device::query_fault_info(gpu::device_fault_counts& counts, gpu::device_fault_info& info) const -> gpu::result {
	std::vector<vk::DeviceFaultAddressInfoEXT> vk_address_infos(counts.address_info_count);
	std::vector<vk::DeviceFaultVendorInfoEXT> vk_vendor_infos(counts.vendor_info_count);
	std::vector<std::byte> vk_vendor_binary(counts.vendor_binary_size);

	vk::DeviceFaultCountsEXT vk_counts{
		.addressInfoCount = counts.address_info_count,
		.vendorInfoCount = counts.vendor_info_count,
		.vendorBinarySize = counts.vendor_binary_size,
	};
	vk::DeviceFaultInfoEXT vk_info{
		.pAddressInfos = vk_address_infos.empty() ? nullptr : vk_address_infos.data(),
		.pVendorInfos = vk_vendor_infos.empty() ? nullptr : vk_vendor_infos.data(),
		.pVendorBinaryData = vk_vendor_binary.empty() ? nullptr : vk_vendor_binary.data(),
	};

	const auto vk_result = m_device.getFaultInfoEXT(&vk_counts, &vk_info);

	counts.address_info_count = vk_counts.addressInfoCount;
	counts.vendor_info_count = vk_counts.vendorInfoCount;
	counts.vendor_binary_size = vk_counts.vendorBinarySize;

	info.description = vk_info.description.data();

	static thread_local std::vector<gpu::device_fault_address_info> tls_addresses;
	tls_addresses.clear();
	tls_addresses.reserve(vk_address_infos.size());
	for (const auto& a : vk_address_infos) {
		tls_addresses.push_back({
			.address_type = static_cast<std::uint32_t>(a.addressType),
			.reported_address = a.reportedAddress,
			.address_precision = a.addressPrecision,
		});
	}
	info.address_infos = tls_addresses;

	static thread_local std::vector<gpu::device_fault_vendor_info> tls_vendors;
	tls_vendors.clear();
	tls_vendors.reserve(vk_vendor_infos.size());
	for (const auto& v : vk_vendor_infos) {
		tls_vendors.push_back({
			.description = v.description.data(),
			.vendor_fault_code = v.vendorFaultCode,
			.vendor_fault_data = v.vendorFaultData,
		});
	}
	info.vendor_infos = tls_vendors;

	static thread_local std::vector<std::byte> tls_binary;
	tls_binary = std::move(vk_vendor_binary);
	info.vendor_binary = tls_binary;

	return from_vk(vk_result);
}

auto gse::vulkan::device::create_buffer(const vk::BufferCreateInfo& buffer_info, const void* data, const std::string_view tag, const std::source_location& loc) -> gpu::buffer {
	auto actual_buffer_info = buffer_info;
	constexpr auto device_addressable_usage = vk::BufferUsageFlagBits::eUniformBuffer |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress;
	const bool needs_device_address = static_cast<bool>(actual_buffer_info.usage & device_addressable_usage);

	if (needs_device_address) {
		actual_buffer_info.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}

	const auto shared_family_indices = m_queue_families;
	if (families_distinct() && actual_buffer_info.sharingMode == vk::SharingMode::eExclusive) {
		actual_buffer_info.sharingMode = vk::SharingMode::eConcurrent;
		actual_buffer_info.queueFamilyIndexCount = static_cast<std::uint32_t>(shared_family_indices.size());
		actual_buffer_info.pQueueFamilyIndices = shared_family_indices.data();
	}

	auto [buffer_result, vk_buffer] = (*m_device).createBuffer(actual_buffer_info, nullptr);
	assert(buffer_result == vk::Result::eSuccess, "failed to create buffer: {}", vk::to_string(buffer_result));
	const auto requirements = (*m_device).getBufferMemoryRequirements(vk_buffer);

	gpu::allocation alloc{};
	bool success = false;

	if (data) {
		constexpr vk::MemoryPropertyFlags required_flags =
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		if (auto expected_alloc = allocate(requirements, required_flags, tag, loc, needs_device_address)) {
			alloc = std::move(*expected_alloc);
			success = true;
		}
	}
	else {
		for (const auto property_preferences = memory_flag_preferences(actual_buffer_info.usage); const auto& props : property_preferences) {
			if (auto expected_alloc = allocate(requirements, props, tag, loc, needs_device_address)) {
				alloc = std::move(*expected_alloc);
				success = true;
				break;
			}
		}
	}

	assert(success, "Failed to allocate memory for buffer after trying all preferences.");

	const auto bind_result = (*m_device).bindBufferMemory(vk_buffer, std::bit_cast<vk::DeviceMemory>(alloc.memory()), alloc.offset());
	assert(bind_result == vk::Result::eSuccess, "failed to bind buffer memory: {}", vk::to_string(bind_result));

	if ((m_settings && m_settings->name_resources)) {
		const auto& debug_info = alloc.debug_info();
		const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
		const auto name = debug_info.tag.empty()
			? std::format("Buffer ({}:{})", file,
						  debug_info.creation_location.line())
			: std::format(
				  "Buffer '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );

		const auto name_result = (*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eBuffer,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(vk_buffer)),
			.pObjectName = name.c_str(),
		});
		assert(name_result == vk::Result::eSuccess, "failed to set buffer debug name: {}", vk::to_string(name_result));
	}

	if (data && alloc.mapped()) {
		gse::memcpy(alloc.mapped(), data, actual_buffer_info.size);
	}
	else if (data && !alloc.mapped()) {
		assert(false, "Buffer created with data, but the allocated memory is not mappable.");
	}

	const auto handle = std::bit_cast<gpu::handle<gpu::buffer>>(vk_buffer);
	const gpu::device_address address = needs_device_address
		? (*m_device).getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = vk_buffer })
		: 0;
	auto* const mapped = alloc.mapped();
	{
		std::lock_guard lock(m_mutex);
		m_live_buffers.emplace(handle.value, live_buffer{ std::move(alloc), {} });
	}
	return gpu::buffer(handle, actual_buffer_info.size, address, mapped);
}

auto gse::vulkan::device::create_buffer(const gpu::buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> gpu::buffer {
	const vk::BufferCreateInfo vk_info{
		.pNext = desc.pnext,
		.size = desc.size,
		.usage = to_vk(desc.usage),
	};
	auto buf = create_buffer(vk_info, desc.data, tag, loc);
	if (!desc.bindless) {
		return buf;
	}
	const auto kind = desc.usage.test(gpu::buffer_flag::uniform) ? gpu::buffer_descriptor_kind::uniform : gpu::buffer_descriptor_kind::storage;
	const auto slot = m_bindless->buffer_pool.allocate();
	write_buffer_descriptor(m_bindless->resource_heap, m_bindless->buffer_pool.offset(slot), kind, buf.device_address(), buf.size_bytes());
	{
		std::lock_guard lock(m_mutex);
		m_live_buffers.at(buf.handle().value).slot = slot;
	}
	return gpu::buffer(buf.handle(), buf.size_bytes(), buf.device_address(), buf.mapped<std::byte>(), slot);
}

auto gse::vulkan::device::create_image(const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, const vk::ImageViewCreateInfo& view_info, const void* data, const std::string_view tag, const std::source_location loc, gpu::image_view_create_info engine_view_info) -> gpu::image {
	auto [image_result, vk_image] = (*m_device).createImage(info, nullptr);
	assert(image_result == vk::Result::eSuccess, "failed to create image: {}", vk::to_string(image_result));
	auto image_guard = make_scope_exit([this, vk_image] {
		(*m_device).destroyImage(vk_image, nullptr);
	});

	const auto requirements = (*m_device).getImageMemoryRequirements(vk_image);

	auto expected_alloc = allocate(requirements, properties, tag, loc);

	if (!expected_alloc) {
		assert(
			false,
			"Failed to allocate memory for image with size {} and usage {} after trying all preferences.",
			requirements.size,
			to_string(info.usage)
		);
	}

	gpu::allocation alloc = std::move(*expected_alloc);

	const auto bind_result = (*m_device).bindImageMemory(vk_image, std::bit_cast<vk::DeviceMemory>(alloc.memory()), alloc.offset());
	assert(bind_result == vk::Result::eSuccess, "failed to bind image memory: {}", vk::to_string(bind_result));

	if (data && alloc.mapped()) {
		gse::memcpy(alloc.mapped(), data, requirements.size);
	}

	vk::ImageView view;

	if (view_info != vk::ImageViewCreateInfo{}) {
		assert(!view_info.image, "Image view info must not have an image set yet!");
		vk::ImageViewCreateInfo actual_view_info = view_info;
		actual_view_info.image = vk_image;
		auto [view_result, created_view] = (*m_device).createImageView(actual_view_info, nullptr);
		assert(view_result == vk::Result::eSuccess, "failed to create image view: {}", vk::to_string(view_result));
		view = created_view;
	}
	else {
		auto [view_result, created_view] = (*m_device).createImageView(
			vk::ImageViewCreateInfo{
				.image = vk_image,
				.viewType = vk::ImageViewType::e2D,
				.format = info.format,
				.subresourceRange = {
					.aspectMask = info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = info.mipLevels,
					.baseArrayLayer = 0,
					.layerCount = info.arrayLayers,
				},
			},
			nullptr
		);
		assert(view_result == vk::Result::eSuccess, "failed to create image view: {}", vk::to_string(view_result));
		view = created_view;
	}

	image_guard.release();

	if (m_settings && m_settings->name_resources) {
		const auto& debug_info = alloc.debug_info();
		const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
		const auto image_name = debug_info.tag.empty()
			? std::format("Image ({}:{})", file, debug_info.creation_location.line())
			: std::format(
				  "Image '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );
		const auto view_name = debug_info.tag.empty()
			? std::format(
				  "ImageView ({}:{})",
				  file,
				  debug_info.creation_location.line()
			  )
			: std::format(
				  "ImageView '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );

		auto name_result = (*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eImage,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(vk_image)),
			.pObjectName = image_name.c_str(),
		});
		assert(name_result == vk::Result::eSuccess, "failed to set image debug name: {}", vk::to_string(name_result));
		name_result = (*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eImageView,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(view)),
			.pObjectName = view_name.c_str(),
		});
		assert(name_result == vk::Result::eSuccess, "failed to set image view debug name: {}", vk::to_string(name_result));
	}

	const auto img_handle = std::bit_cast<gpu::handle<gpu::image>>(vk_image);
	const auto view_handle = std::bit_cast<gpu::handle<gpu::image_view>>(view);
	{
		std::lock_guard lock(m_mutex);
		m_live_images.emplace(img_handle.value, live_image{ std::move(alloc), view_handle });
	}
	return gpu::image(
		img_handle,
		view_handle,
		static_cast<gpu::image_format_value>(info.format),
		vec3u{ info.extent.width, info.extent.height, info.extent.depth },
		engine_view_info
	);
}

auto gse::vulkan::device::create_image(const gpu::image_create_info& info, const gpu::memory_property_flags properties, const gpu::image_view_create_info& view_info, const void* data, const std::string_view tag, const std::source_location loc) -> gpu::image {
	const vk::ImageCreateInfo vk_info{
		.flags = to_vk(info.flags),
		.imageType = to_vk(info.type),
		.format = to_vk(info.format),
		.extent = vk::Extent3D{ info.extent.x(), info.extent.y(), info.extent.z() },
		.mipLevels = info.mip_levels,
		.arrayLayers = info.array_layers,
		.samples = to_vk(info.samples),
		.tiling = vk::ImageTiling::eOptimal,
		.usage = to_vk(info.usage),
	};
	auto aspects = view_info.aspects;
	if (!aspects) {
		aspects = info.usage.test(gpu::image_flag::depth_attachment)
			? gpu::image_aspect_flags{ gpu::image_aspect_flag::depth }
			: gpu::image_aspect_flags{ gpu::image_aspect_flag::color };
	}
	const vk::ImageViewCreateInfo vk_view_info{
		.viewType = to_vk(view_info.view_type),
		.format = to_vk(view_info.format == gpu::image_format::r8g8b8a8_unorm ? info.format : view_info.format),
		.subresourceRange = {
			.aspectMask = to_vk(aspects),
			.baseMipLevel = view_info.base_mip_level,
			.levelCount = view_info.level_count == 1 ? info.mip_levels : view_info.level_count,
			.baseArrayLayer = view_info.base_array_layer,
			.layerCount = view_info.layer_count == 1 ? info.array_layers : view_info.layer_count,
		},
	};
	auto resolved_view_info = view_info;
	resolved_view_info.aspects = aspects;
	if (view_info.level_count == 1) {
		resolved_view_info.level_count = info.mip_levels;
	}
	if (view_info.layer_count == 1) {
		resolved_view_info.layer_count = info.array_layers;
	}
	if (view_info.format == gpu::image_format::r8g8b8a8_unorm) {
		resolved_view_info.format = info.format;
	}
	return create_image(vk_info, to_vk(properties), vk_view_info, data, tag, loc, resolved_view_info);
}

auto gse::vulkan::device::create_image(const gpu::image_desc& desc, const std::string_view tag, const std::source_location& loc) -> gpu::image {
	const bool is_depth = desc.format == gpu::image_format::d32_sfloat;
	const bool is_cube = desc.view == gpu::image_view_type::cube;
	const bool is_3d = desc.view == gpu::image_view_type::e3d;
	const std::uint32_t layers = is_cube ? 6u : 1u;
	const gpu::image_type type = is_3d ? gpu::image_type::e3d : gpu::image_type::e2d;
	const std::uint32_t z_extent = is_3d ? desc.depth : 1u;

	auto usage = desc.usage;
	if (usage.test(gpu::image_flag::transfer_dst)) {
		usage |= gpu::image_flag::host_transfer;
	}

	const gpu::image_create_info create_info{
		.flags =
			is_cube ? gpu::image_create_flags{ gpu::image_create_flag::cube_compatible } : gpu::image_create_flags{},
		.type = type,
		.format = desc.format,
		.extent = vec3u{ desc.size.x(), desc.size.y(), z_extent },
		.mip_levels = 1,
		.array_layers = layers,
		.samples = gpu::sample_count::e1,
		.usage = usage,
	};
	const gpu::image_view_create_info view_info{
		.format = desc.format,
		.view_type = desc.view,
		.aspects = is_depth ? gpu::image_aspect_flags{ gpu::image_aspect_flag::depth }
							: gpu::image_aspect_flags{ gpu::image_aspect_flag::color },
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layers,
	};

	auto img = create_image(create_info, gpu::memory_property_flag::device_local, view_info, nullptr, tag, loc);
	if (!desc.bindless) {
		return img;
	}
	const auto storage_slot = m_bindless->image_pool.allocate();
	const auto sampled_slot = m_bindless->image_pool.allocate();
	write_image_descriptor(m_bindless->resource_heap, m_bindless->image_pool.offset(storage_slot), gpu::image_descriptor_kind::storage, img);
	write_image_descriptor(m_bindless->resource_heap, m_bindless->image_pool.offset(sampled_slot), gpu::image_descriptor_kind::sampled, img);
	{
		std::lock_guard lock(m_mutex);
		auto& li = m_live_images.at(img.handle().value);
		li.storage_slot = storage_slot;
		li.sampled_slot = sampled_slot;
	}
	return gpu::image(img.handle(), img.view(), img.format(), img.extent(), img.view_create_info(), storage_slot, sampled_slot);
}

auto gse::vulkan::device::live_allocation_count() const -> std::uint32_t {
	return m_live_allocation_count.load();
}

auto gse::vulkan::device::tracking_enabled() const -> bool {
	return (m_settings && m_settings->tracking_enabled);
}

auto gse::vulkan::device::destroy_buffer(const gpu::handle<gpu::buffer> buffer) -> void {
	gpu::allocation alloc;
	{
		std::lock_guard lock(m_mutex);
		if (const auto it = m_live_buffers.find(buffer.value); it != m_live_buffers.end()) {
			alloc = std::move(it->second.alloc);
			if (m_bindless && it->second.slot.valid()) {
				m_bindless->buffer_pool.release(it->second.slot);
			}
			m_live_buffers.erase(it);
		}
	}
	free_allocation(alloc);
	(*m_device).destroyBuffer(std::bit_cast<vk::Buffer>(buffer), nullptr);
}

auto gse::vulkan::device::retire(const gpu::handle<gpu::buffer> buffer) -> void {
	std::lock_guard lock(m_mutex);
	m_retired_buffers.push_back({ buffer.value, m_resource_frame + gpu::max_frames_in_flight });
}

auto gse::vulkan::device::retire(const gpu::handle<gpu::image> image) -> void {
	std::lock_guard lock(m_mutex);
	m_retired_images.push_back({ image.value, m_resource_frame + gpu::max_frames_in_flight });
}

auto gse::vulkan::device::retire(const gpu::acceleration_structure acceleration_structure) -> void {
	std::lock_guard lock(m_mutex);
	m_owned.retire(acceleration_structure, m_resource_frame + gpu::max_frames_in_flight);
}

auto gse::vulkan::device::retire(const gpu::handle<gpu::semaphore> semaphore) -> void {
	std::lock_guard lock(m_mutex);
	m_owned.retire(semaphore, m_resource_frame + gpu::max_frames_in_flight);
}

auto gse::vulkan::device::retire(const gpu::handle<gpu::fence> fence) -> void {
	std::lock_guard lock(m_mutex);
	m_owned.retire(fence, m_resource_frame + gpu::max_frames_in_flight);
}

auto gse::vulkan::device::collect_garbage() -> void {
	std::vector<std::uint64_t> buffers;
	std::vector<std::uint64_t> images;
	{
		std::lock_guard lock(m_mutex);
		++m_resource_frame;
		std::erase_if(m_retired_buffers, [&](const gpu::retired_resource& r) {
			if (m_resource_frame >= r.retire_after) {
				buffers.push_back(r.value);
				return true;
			}
			return false;
		});
		std::erase_if(m_retired_images, [&](const gpu::retired_resource& r) {
			if (m_resource_frame >= r.retire_after) {
				images.push_back(r.value);
				return true;
			}
			return false;
		});
		m_owned.collect(m_resource_frame);
	}
	for (const auto value : buffers) {
		destroy_buffer(std::bit_cast<gpu::handle<gpu::buffer>>(value));
	}
	for (const auto value : images) {
		destroy_image(std::bit_cast<gpu::handle<gpu::image>>(value));
	}
}

auto gse::vulkan::device::buffer_device_address(const gpu::handle<gpu::buffer> buffer) const -> gpu::device_address {
	return (*m_device).getBufferAddress(vk::BufferDeviceAddressInfo{
		.buffer = std::bit_cast<vk::Buffer>(buffer),
	});
}

auto gse::vulkan::device::destroy_image(const gpu::handle<gpu::image> image) -> void {
	live_image li;
	bool found = false;
	{
		std::lock_guard lock(m_mutex);
		if (const auto it = m_live_images.find(image.value); it != m_live_images.end()) {
			li = std::move(it->second);
			found = true;
			m_live_images.erase(it);
		}
	}
	if (found) {
		if (m_bindless) {
			if (li.storage_slot.valid()) {
				m_bindless->image_pool.release(li.storage_slot);
			}
			if (li.sampled_slot.valid()) {
				m_bindless->image_pool.release(li.sampled_slot);
			}
		}
		(*m_device).destroyImageView(std::bit_cast<vk::ImageView>(li.view), nullptr);
		free_allocation(li.alloc);
	}
	(*m_device).destroyImage(std::bit_cast<vk::Image>(image), nullptr);
}

auto gse::vulkan::device::destroy_image_view(const gpu::handle<gpu::image_view> view) const -> void {
	(*m_device).destroyImageView(std::bit_cast<vk::ImageView>(view), nullptr);
}

auto gse::vulkan::device::create_image_unbound(const gpu::image_create_info& info) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements> {
	const vk::ImageCreateInfo vk_info{
		.flags = to_vk(info.flags),
		.imageType = to_vk(info.type),
		.format = to_vk(info.format),
		.extent = vk::Extent3D{ info.extent.x(), info.extent.y(), info.extent.z() },
		.mipLevels = info.mip_levels,
		.arrayLayers = info.array_layers,
		.samples = to_vk(info.samples),
		.tiling = vk::ImageTiling::eOptimal,
		.usage = to_vk(info.usage),
	};
	auto [image_result, vk_image] = (*m_device).createImage(vk_info, nullptr);
	assert(image_result == vk::Result::eSuccess, "failed to create image: {}", vk::to_string(image_result));
	const auto reqs = (*m_device).getImageMemoryRequirements(vk_image);
	return {
		std::bit_cast<gpu::handle<gpu::image>>(vk_image),
		gpu::memory_requirements{
			.size = reqs.size,
			.alignment = reqs.alignment,
			.memory_type_bits = reqs.memoryTypeBits,
		},
	};
}

auto gse::vulkan::device::create_buffer_unbound(const gpu::buffer_desc& info) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements> {
	const vk::BufferCreateInfo vk_info{
		.pNext = info.pnext,
		.size = info.size,
		.usage = to_vk(info.usage),
	};
	auto [buffer_result, vk_buffer] = (*m_device).createBuffer(vk_info, nullptr);
	assert(buffer_result == vk::Result::eSuccess, "failed to create buffer: {}", vk::to_string(buffer_result));
	const auto reqs = (*m_device).getBufferMemoryRequirements(vk_buffer);
	return {
		std::bit_cast<gpu::handle<gpu::buffer>>(vk_buffer),
		gpu::memory_requirements{
			.size = reqs.size,
			.alignment = reqs.alignment,
			.memory_type_bits = reqs.memoryTypeBits,
		},
	};
}

auto gse::vulkan::device::bind_image_memory(const gpu::handle<gpu::image> img, const gpu::device_memory mem, const gpu::device_size offset) const -> void {
	const auto result = (*m_device).bindImageMemory(std::bit_cast<vk::Image>(img), std::bit_cast<vk::DeviceMemory>(mem.value), offset);
	assert(result == vk::Result::eSuccess, "failed to bind image memory: {}", vk::to_string(result));
}

auto gse::vulkan::device::bind_buffer_memory(const gpu::handle<gpu::buffer> buf, const gpu::device_memory mem, const gpu::device_size offset) const -> void {
	const auto result = (*m_device).bindBufferMemory(std::bit_cast<vk::Buffer>(buf), std::bit_cast<vk::DeviceMemory>(mem.value), offset);
	assert(result == vk::Result::eSuccess, "failed to bind buffer memory: {}", vk::to_string(result));
}

auto gse::vulkan::device::create_image_view(const gpu::handle<gpu::image> img, const gpu::image_view_create_info& info) const -> gpu::handle<gpu::image_view> {
	const vk::ImageViewCreateInfo vk_info{
		.image = std::bit_cast<vk::Image>(img),
		.viewType = to_vk(info.view_type),
		.format = to_vk(info.format),
		.subresourceRange = {
			.aspectMask = to_vk(info.aspects),
			.baseMipLevel = info.base_mip_level,
			.levelCount = info.level_count,
			.baseArrayLayer = info.base_array_layer,
			.layerCount = info.layer_count,
		},
	};
	auto [view_result, vk_view] = (*m_device).createImageView(vk_info, nullptr);
	assert(view_result == vk::Result::eSuccess, "failed to create image view: {}", vk::to_string(view_result));
	return std::bit_cast<gpu::handle<gpu::image_view>>(vk_view);
}

auto gse::vulkan::device::allocate_aliased_memory(const gpu::device_size size, const std::uint32_t memory_type_index) const -> gpu::device_memory {
	const vk::MemoryAllocateInfo alloc_info{
		.allocationSize = size,
		.memoryTypeIndex = memory_type_index,
	};
	auto [memory_result, vk_memory] = (*m_device).allocateMemory(alloc_info);
	assert(memory_result == vk::Result::eSuccess, "failed to allocate memory: {}", vk::to_string(memory_result));
	return {
		.value = std::bit_cast<std::uint64_t>(vk_memory)
	};
}

auto gse::vulkan::device::free_aliased_memory(const gpu::device_memory mem) const -> void {
	if (!mem) {
		return;
	}
	(*m_device).freeMemory(std::bit_cast<vk::DeviceMemory>(mem.value), nullptr);
}

auto gse::vulkan::device::find_memory_type_index(const std::uint32_t type_bits, const gpu::memory_property_flags required) const -> std::uint32_t {
	const auto vk_required = to_vk(required);
	const auto props = m_physical_device.memory_properties();
	for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
		if (!(type_bits & (1u << i))) {
			continue;
		}
		if ((props.memoryTypes[i].propertyFlags & vk_required) != vk_required) {
			continue;
		}
		return i;
	}
	assert(false, "no compatible memory type for type_bits={:#x}", type_bits);
	return 0;
}

auto gse::vulkan::device::free_allocation(const gpu::allocation& alloc) -> void {
	std::lock_guard lock(m_mutex);

	if (!alloc.owner()) {
		return;
	}

	if ((m_settings && m_settings->tracking_enabled)) {
		const auto& [creation_location, tag, allocation_id] = alloc.debug_info();
		assert(
			!m_cleaned_up,
			"Allocation freed after allocator cleanup! Tag: '{}', Created at: {}:{}:{}",
			tag,
			creation_location.file_name(),
			creation_location.line(),
			creation_location.function_name()
		);

		if (allocation_id != 0) {
			m_live_allocations.erase(allocation_id);
			--m_live_allocation_count;
		}
	}

	const auto* sub_to_free = alloc.owner();

	for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (auto& block : blocks) {
			for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
				if (&*it == sub_to_free) {
					it->in_use = false;

					if (const auto next_it = std::next(it); next_it != block.allocations.end() && !next_it->in_use) {
						it->size += next_it->size;
						block.allocations.erase(next_it);
					}

					if (it != block.allocations.begin()) {
						if (const auto prev_it = std::prev(it); !prev_it->in_use) {
							prev_it->size += it->size;
							block.allocations.erase(it);
							return;
						}
					}
					return;
				}
			}
		}
	}
}

auto gse::vulkan::query_descriptor_heap_props(const physical_device& pd) -> gpu::descriptor_heap_properties {
	const auto chain = std::bit_cast<vk::PhysicalDevice>(pd.handle())
		.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
	const auto& dh = chain.get<vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
	return {
		.sampler_heap_alignment = dh.samplerHeapAlignment,
		.resource_heap_alignment = dh.resourceHeapAlignment,
		.max_sampler_heap_size = dh.maxSamplerHeapSize,
		.max_resource_heap_size = dh.maxResourceHeapSize,
		.min_sampler_heap_reserved_range = dh.minSamplerHeapReservedRange,
		.min_resource_heap_reserved_range = dh.minResourceHeapReservedRange,
		.sampler_descriptor_size = dh.samplerDescriptorSize,
		.image_descriptor_size = dh.imageDescriptorSize,
		.buffer_descriptor_size = dh.bufferDescriptorSize,
		.sampler_descriptor_alignment = dh.samplerDescriptorAlignment,
		.image_descriptor_alignment = dh.imageDescriptorAlignment,
		.buffer_descriptor_alignment = dh.bufferDescriptorAlignment,
		.max_push_data_size = dh.maxPushDataSize,
		.max_embedded_samplers = dh.maxDescriptorHeapEmbeddedSamplers,
		.sparse_descriptor_heaps = static_cast<bool>(dh.sparseDescriptorHeaps),
	};
}

gse::vulkan::device::device(class physical_device&& physical_device, vk::raii::Device&& device, gpu::device_settings& cfg, const bool device_fault_enabled, const bool device_fault_vendor_binary_enabled, const std::uint32_t graphics_family, const std::uint32_t compute_family, const gpu::surface surface)
	: m_physical_device(std::move(physical_device)), m_device(std::move(device)), m_fault_enabled(device_fault_enabled), m_vendor_binary_fault_enabled(device_fault_vendor_binary_enabled), m_settings(&cfg) {
	m_queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)] = graphics_family;
	m_queue_families[static_cast<std::size_t>(gpu::queue_type::compute)] = compute_family;
	m_surface = surface;
	m_descriptor_heap_props = query_descriptor_heap_props(m_physical_device);
}

auto gse::vulkan::device::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties, const std::string_view tag, const std::source_location loc, const bool device_address) -> std::expected<gpu::allocation, std::string> {
	std::lock_guard lock(m_mutex);

	if ((m_settings && m_settings->tracking_enabled)) {
		assert(
			!m_cleaned_up,
			"Attempted to allocate after allocator cleanup! This indicates a resource lifetime issue."
		);
	}

	const auto mem_props = m_physical_device.memory_properties();

	std::uint32_t new_memory_type_index = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((requirements.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			new_memory_type_index = i;
			break;
		}
	}

	if (new_memory_type_index == std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected{ "Failed to find suitable memory type for allocation!" };
	}

	auto& [memory_type_index, blocks] = m_pools[{ new_memory_type_index, properties, device_address }];
	if (blocks.empty()) {
		memory_type_index = new_memory_type_index;
	}

	memory_block* best_block = nullptr;
	std::list<gpu::sub_allocation>::iterator best_sub_it;
	vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
	vk::DeviceSize best_aligned_offset = 0;

	if (!blocks.empty()) {
		for (auto& block : blocks) {
			for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
				if (it->in_use) {
					continue;
				}

				const vk::DeviceSize aligned_offset = (it->offset + requirements.alignment - 1) & ~(requirements.alignment - 1);
				if (aligned_offset + requirements.size > it->offset + it->size) {
					continue;
				}

				if (const vk::DeviceSize leftover = it->size - (aligned_offset - it->offset) - requirements.size; leftover < best_leftover) {
					best_block = &block;
					best_sub_it = it;
					best_leftover = leftover;
					best_aligned_offset = aligned_offset;
					if (leftover == 0) {
						break;
					}
				}
			}
			if (best_leftover == 0) {
				break;
			}
		}
	}

	if (best_block) {
		auto& list = best_block->allocations;
		const auto sub = *best_sub_it;

		const vk::DeviceSize prefix_size = best_aligned_offset - sub.offset;
		const vk::DeviceSize suffix_size = sub.size - prefix_size - requirements.size;

		const auto next_it = list.erase(best_sub_it);
		if (prefix_size) {
			list.insert(
				next_it,
				{ sub.offset, prefix_size, false }
			);
		}
		const auto owner_it = list.insert(
			next_it,
			{ best_aligned_offset, requirements.size, true }
		);
		if (suffix_size) {
			list.insert(
				next_it,
				{ best_aligned_offset + requirements.size, suffix_size, false }
			);
		}

		gpu::allocation_debug_info debug_info;
		debug_info.creation_location = loc;
		debug_info.tag = tag;
		if ((m_settings && m_settings->tracking_enabled)) {
			debug_info.allocation_id = m_next_allocation_id++;
			m_live_allocations[debug_info.allocation_id] = debug_info;
			++m_live_allocation_count;
		}

		return gpu::allocation{ std::bit_cast<std::uint64_t>(best_block->memory),
										 requirements.size,
										 best_aligned_offset,
										 best_block->mapped
											 ? static_cast<char*>(best_block->mapped) + best_aligned_offset
											 : nullptr,
										 &*owner_it,
										 std::move(debug_info) };
	}

	const vk::DeviceSize aligned_offset = (0 + requirements.alignment - 1) & ~(requirements.alignment - 1);
	const vk::DeviceSize required_space = aligned_offset + requirements.size;
	const auto new_block_size = std::max(required_space, k_default_block_size);

	const vk::MemoryAllocateFlagsInfo flags_info{
		.flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
	};
	const vk::MemoryAllocateInfo alloc_info{
		.pNext = device_address ? static_cast<const void*>(&flags_info) : nullptr,
		.allocationSize = new_block_size,
		.memoryTypeIndex = new_memory_type_index,
	};
	auto [memory_result, memory] = (*m_device).allocateMemory(alloc_info, nullptr);
	assert(memory_result == vk::Result::eSuccess, "failed to allocate memory block: {}", vk::to_string(memory_result));

	blocks.emplace_back(memory, new_block_size, properties);

	auto& new_block = blocks.back();

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		auto [map_result, mapped_ptr] = (*m_device).mapMemory(
			new_block.memory,
			0,
			new_block.size,
			{}
		);
		assert(map_result == vk::Result::eSuccess, "failed to map memory block: {}", vk::to_string(map_result));
		new_block.mapped = mapped_ptr;
	}

	const vk::DeviceSize prefix_size = aligned_offset;
	const vk::DeviceSize suffix_size = new_block.size - required_space;

	if (prefix_size > 0) {
		new_block.allocations.push_back({ 0, prefix_size, false });
	}
	new_block.allocations.push_back({ aligned_offset, requirements.size, true });
	gpu::sub_allocation* owner_ptr = &new_block.allocations.back();
	if (suffix_size > 0) {
		new_block.allocations.push_back({ aligned_offset + requirements.size, suffix_size, false });
	}

	gpu::allocation_debug_info debug_info;
	debug_info.creation_location = loc;
	debug_info.tag = tag;
	if ((m_settings && m_settings->tracking_enabled)) {
		debug_info.allocation_id = m_next_allocation_id++;
		m_live_allocations[debug_info.allocation_id] = debug_info;
		++m_live_allocation_count;
	}

	return gpu::allocation{ std::bit_cast<std::uint64_t>(new_block.memory),
									 requirements.size,
									 aligned_offset,
									 new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
									 owner_ptr,
									 std::move(debug_info) };
}

auto gse::vulkan::device::clean_up() -> void {
	std::lock_guard lock(m_mutex);

	if (!*m_device) {
		return;
	}

	m_owned.clear();

	for (auto& [handle, lb] : m_live_buffers) {
		(*m_device).destroyBuffer(std::bit_cast<vk::Buffer>(handle), nullptr);
		if (lb.alloc.owner()) {
			lb.alloc.owner()->in_use = false;
		}
	}
	m_live_buffers.clear();
	for (auto& [handle, li] : m_live_images) {
		(*m_device).destroyImageView(std::bit_cast<vk::ImageView>(li.view), nullptr);
		(*m_device).destroyImage(std::bit_cast<vk::Image>(handle), nullptr);
		if (li.alloc.owner()) {
			li.alloc.owner()->in_use = false;
		}
	}
	m_live_images.clear();

	std::uint32_t leaked_sub_allocations = 0;
	for (const auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (const auto& block : blocks) {
			for (const auto& sub : block.allocations) {
				if (sub.in_use) {
					++leaked_sub_allocations;
				}
			}
		}
	}

	if (leaked_sub_allocations > 0) {
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"{} sub-allocations still in use at allocator cleanup!",
			leaked_sub_allocations
		);
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"Resources are being destroyed after their memory is freed."
		);
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"Destroy all gpu::image and gpu::buffer instances before "
			"the device."
		);

		if ((m_settings && m_settings->tracking_enabled) && !m_live_allocations.empty()) {
			log::println(log::level::error, log::category::vulkan_memory, "Tracked allocations still alive:");
			for (const auto& [id, debug] : m_live_allocations) {
				log::println(
					log::level::error,
					log::category::vulkan_memory,
					"- [{}] '{}' created at {}:{}:{}",
					id,
					debug.tag.empty() ? "(no tag)" : debug.tag,
					debug.creation_location.file_name(),
					debug.creation_location.line(),
					debug.creation_location.function_name()
				);
			}
		}
		else if (!(m_settings && m_settings->tracking_enabled)) {
			log::println(
				log::level::warning,
				log::category::vulkan_memory,
				"Enable 'Vulkan.Track Allocations' for detailed allocation info."
			);
		}

		assert(
			false,
			"Device cleanup called with {} leaked sub-allocations - destroy resources before the device!",
			leaked_sub_allocations
		);
	}

	m_cleaned_up = true;

	for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (auto& block : blocks) {
			if (block.mapped) {
				(*m_device).unmapMemory(block.memory);
				block.mapped = nullptr;
			}
			if (block.memory) {
				(*m_device).freeMemory(block.memory, nullptr);
			}
		}
	}
	m_pools.clear();
}

auto gse::vulkan::device::memory_flag_preferences(const vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags> {
	using mpf = vk::MemoryPropertyFlagBits;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer || usage & vk::BufferUsageFlagBits::eIndexBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eDeviceLocal,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eUniformBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eHostVisible,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eDeviceLocal,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
		return { mpf::eDeviceLocal };
	}

	return { mpf::eHostVisible | mpf::eHostCoherent };
}

auto gse::vulkan::device::pool_key::operator==(const pool_key& other) const -> bool {
	return memory_type_index == other.memory_type_index && properties == other.properties &&
		device_address == other.device_address;
}

auto gse::vulkan::device::pool_key_hash::operator()(const pool_key& key) const noexcept -> std::size_t {
	const auto memory_hash = std::hash<std::uint32_t>()(key.memory_type_index);
	const auto props_hash =
		std::hash<vk::MemoryPropertyFlags::MaskType>()(static_cast<vk::MemoryPropertyFlags::MaskType>(key.properties));
	const auto address_hash = std::hash<bool>()(key.device_address);
	return memory_hash ^ (props_hash << 1) ^ (address_hash << 2);
}

namespace gse::vulkan {
	auto to_vk_geometry(const gpu::acceleration_structure_geometry& g) -> vk::AccelerationStructureGeometryKHR {
		vk::AccelerationStructureGeometryDataKHR data{};
		vk::GeometryTypeKHR vk_type = vk::GeometryTypeKHR::eInstances;
		if (g.type == gpu::acceleration_structure_geometry_type::triangles) {
			vk_type = vk::GeometryTypeKHR::eTriangles;
			data.triangles = vk::AccelerationStructureGeometryTrianglesDataKHR{
				.vertexFormat = to_vk(g.triangles.vertex_format),
				.vertexData =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.triangles.vertex_data
					},
				.vertexStride = g.triangles.vertex_stride,
				.maxVertex = g.triangles.max_vertex,
				.indexType = to_vk(g.triangles.index_type),
				.indexData =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.triangles.index_data
					},
			};
		}
		else {
			data.instances = vk::AccelerationStructureGeometryInstancesDataKHR{
				.arrayOfPointers = g.instances.array_of_pointers ? vk::True : vk::False,
				.data =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.instances.data
					},
			};
		}
		return vk::AccelerationStructureGeometryKHR{
			.geometryType = vk_type,
			.geometry = data,
			.flags = to_vk(g.flags),
		};
	}
}

auto gse::vulkan::device::query_blas_build_sizes(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> gpu::acceleration_structure_build_sizes {
	const auto vk_geometry = to_vk_geometry(geometry);
	const vk::AccelerationStructureBuildGeometryInfoKHR sizing_info{
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &vk_geometry,
	};
	const auto sizes = raii_device().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		sizing_info,
		prim_count
	);
	return {
		.acceleration_structure_size = sizes.accelerationStructureSize,
		.build_scratch_size = sizes.buildScratchSize,
		.update_scratch_size = sizes.updateScratchSize,
	};
}

auto gse::vulkan::device::query_tlas_build_sizes(const std::uint32_t max_instances) const -> gpu::acceleration_structure_build_sizes {
	constexpr vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False,
	};
	constexpr vk::AccelerationStructureGeometryKHR vk_geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry = {
			.instances = instances_data
		},
	};
	const vk::AccelerationStructureBuildGeometryInfoKHR sizing_info{
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild |
			vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &vk_geometry,
	};
	const auto sizes = raii_device().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		sizing_info,
		max_instances
	);
	return {
		.acceleration_structure_size = sizes.accelerationStructureSize,
		.build_scratch_size = sizes.buildScratchSize,
		.update_scratch_size = sizes.updateScratchSize,
	};
}

template <typename Frontend, typename Raii>
auto gse::vulkan::device::adopt(Raii&& object) -> Frontend {
	const auto handle = std::bit_cast<Frontend>(*object);
	std::lock_guard lock(m_mutex);
	m_owned.store(handle.value, std::forward<Raii>(object));
	return handle;
}

auto gse::vulkan::device::create_acceleration_structure(const gpu::handle<gpu::buffer> storage_buffer, const gpu::device_size size, const gpu::acceleration_structure_type type) -> gpu::acceleration_structure {
	auto [as_result, acceleration_structure] = raii_device().createAccelerationStructureKHR({
		.buffer = std::bit_cast<vk::Buffer>(storage_buffer),
		.size = size,
		.type = to_vk(type),
	});
	assert(as_result == vk::Result::eSuccess, "failed to create acceleration structure: {}", vk::to_string(as_result));
	return adopt<gpu::acceleration_structure>(std::move(acceleration_structure));
}

auto gse::vulkan::device::acceleration_structure_address(const gpu::acceleration_structure acceleration_structure) const -> gpu::device_address {
	return raii_device().getAccelerationStructureAddressKHR({
		.accelerationStructure = std::bit_cast<vk::AccelerationStructureKHR>(acceleration_structure),
	});
}

auto gse::vulkan::device::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return m_physical_device.scratch_offset_alignment();
}

auto gse::vulkan::device::create_semaphore() -> gpu::handle<gpu::semaphore> {
	auto [semaphore_result, semaphore] = raii_device().createSemaphore(vk::SemaphoreCreateInfo{});
	assert(semaphore_result == vk::Result::eSuccess, "failed to create semaphore: {}", vk::to_string(semaphore_result));
	return adopt<gpu::handle<gpu::semaphore>>(std::move(semaphore));
}

auto gse::vulkan::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	const auto flags = signaled ? vk::FenceCreateFlags{ vk::FenceCreateFlagBits::eSignaled } : vk::FenceCreateFlags{};
	auto [fence_result, fence] = raii_device().createFence(vk::FenceCreateInfo{ .flags = flags });
	assert(fence_result == vk::Result::eSuccess, "failed to create fence: {}", vk::to_string(fence_result));
	return adopt<gpu::handle<gpu::fence>>(std::move(fence));
}

auto gse::vulkan::device::begin_one_time_commands(const gpu::command_buffer_handle cmd) -> void {
	constexpr vk::CommandBufferBeginInfo begin_info{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};
	const auto result = std::bit_cast<vk::CommandBuffer>(cmd).begin(begin_info);
	assert(result == vk::Result::eSuccess, "failed to begin command buffer: {}", vk::to_string(result));
}

auto gse::vulkan::device::end_commands(const gpu::command_buffer_handle cmd) -> void {
	const auto result = std::bit_cast<vk::CommandBuffer>(cmd).end();
	assert(result == vk::Result::eSuccess, "failed to end command buffer: {}", vk::to_string(result));
}

auto gse::vulkan::device::create_transient_command_pool(const std::uint32_t family) -> gpu::transient_pool_handle {
	auto pool = gpu::must(transient_command_pool::create(raii_device(), family));
	const auto index = static_cast<std::uint32_t>(m_transient_pools.size());
	m_transient_pools.push_back(std::move(pool));
	return gpu::transient_pool_handle{ .index = index };
}

auto gse::vulkan::device::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	return m_transient_pools[pool.index].allocate_primary(raii_device());
}

auto gse::vulkan::device::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	m_transient_pools[pool.index].try_reset(queue_progress);
}

auto gse::vulkan::device::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	m_transient_pools[pool.index].mark_in_use_until(value);
}

auto gse::vulkan::device::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	m_transient_pools[pool.index].reset_all();
}

gse::vulkan::transient_command_pool::transient_command_pool(vk::raii::CommandPool&& pool) : m_pool(std::move(pool)) {
}

auto gse::vulkan::transient_command_pool::create(const vk::raii::Device& vk_device, const std::uint32_t family) -> gpu::expected<transient_command_pool> {
	const vk::CommandPoolCreateInfo pool_info{
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = family,
	};
	auto [result, pool] = vk_device.createCommandPool(pool_info);
	if (result != vk::Result::eSuccess) {
		return std::unexpected(from_vk(result));
	}
	return transient_command_pool(std::move(pool));
}

auto gse::vulkan::transient_command_pool::allocate_primary(const vk::raii::Device& vk_device) -> gpu::command_buffer_handle {
	if (m_used == m_owned_cbs.size()) {
		const vk::CommandBufferAllocateInfo alloc_info{
			.commandPool = *m_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = allocation_batch_size,
		};
		auto [result, fresh] = vk_device.allocateCommandBuffers(alloc_info);
		assert(result == vk::Result::eSuccess, "failed to allocate transient command buffers: {}", vk::to_string(result));
		for (auto& cb : fresh) {
			m_owned_cbs.push_back(std::move(cb));
		}
	}
	return std::bit_cast<gpu::command_buffer_handle>(*m_owned_cbs[m_used++]);
}

auto gse::vulkan::transient_command_pool::try_reset(const std::uint64_t queue_progress) -> void {
	if (m_used == 0) {
		return;
	}
	if (queue_progress < m_high_water_mark) {
		return;
	}
	m_pool.reset();
	m_used = 0;
}

auto gse::vulkan::transient_command_pool::mark_in_use_until(const std::uint64_t value) -> void {
	if (value > m_high_water_mark) {
		m_high_water_mark = value;
	}
}

auto gse::vulkan::transient_command_pool::reset_all() -> void {
	m_pool.reset();
	m_used = 0;
	m_high_water_mark = 0;
}

auto gse::vulkan::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	const vk::SemaphoreTypeCreateInfo type_info{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = initial_value,
	};
	auto [semaphore_result, semaphore] = raii_device().createSemaphore(vk::SemaphoreCreateInfo{
		.pNext = &type_info,
	});
	assert(semaphore_result == vk::Result::eSuccess, "failed to create timeline semaphore: {}", vk::to_string(semaphore_result));
	return adopt<gpu::handle<gpu::semaphore>>(std::move(semaphore));
}

auto gse::vulkan::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	auto [result, value] = (*m_device).getSemaphoreCounterValue(std::bit_cast<vk::Semaphore>(semaphore));
	assert(result == vk::Result::eSuccess, "failed to get semaphore counter value: {}", vk::to_string(result));
	return value;
}

auto gse::vulkan::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	const auto vk_semaphore = std::bit_cast<vk::Semaphore>(semaphore);
	(void)raii_device().waitSemaphores(
		vk::SemaphoreWaitInfo{
			.semaphoreCount = 1,
			.pSemaphores = &vk_semaphore,
			.pValues = &value,
		},
		std::numeric_limits<std::uint64_t>::max()
	);
}

auto gse::vulkan::device::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	auto [result, pool] = raii_device().createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = capacity,
	});
	assert(result == vk::Result::eSuccess, "failed to create timestamp query pool: {}", vk::to_string(result));
	if (!label.empty()) {
		const std::string name{ label };
		(void)raii_device().setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eQueryPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = name.c_str(),
		});
	}
	return adopt<gpu::handle<gpu::query_pool>>(std::move(pool));
}

auto gse::vulkan::device::create_pipeline_stats_query_pool(const std::uint32_t capacity, const gpu::pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	auto [result, pool] = raii_device().createQueryPool({
		.queryType = vk::QueryType::ePipelineStatistics,
		.queryCount = capacity,
		.pipelineStatistics = to_vk(statistics),
	});
	assert(result == vk::Result::eSuccess, "failed to create pipeline stats query pool: {}", vk::to_string(result));
	if (!label.empty()) {
		const std::string name{ label };
		(void)raii_device().setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eQueryPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = name.c_str(),
		});
	}
	return adopt<gpu::handle<gpu::query_pool>>(std::move(pool));
}

auto gse::vulkan::device::query_pool_results(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>> {
	const auto* vk_pool = m_owned.find(pool);
	if (!vk_pool) {
		return { gpu::query_status::error, {} };
	}
	const std::size_t group_count = stride > 0 ? std::max<std::size_t>(stride / sizeof(std::uint64_t), 1) : 1;
	const std::size_t element_count = static_cast<std::size_t>(query_count) * group_count;
	auto [status, values] = vk_pool->getResults<std::uint64_t>(
		first_query,
		query_count,
		element_count * sizeof(std::uint64_t),
		stride,
		vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
	);
	return { status == vk::Result::eSuccess ? gpu::query_status::success : gpu::query_status::error, std::move(values) };
}

auto gse::vulkan::device::create_sampler(const gpu::sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	const vk::SamplerCreateInfo info{
		.magFilter = to_vk(desc.mag),
		.minFilter = to_vk(desc.min),
		.mipmapMode = desc.min == gpu::sampler_filter::nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear,
		.addressModeU = to_vk(desc.address_u),
		.addressModeV = to_vk(desc.address_v),
		.addressModeW = to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = to_vk(desc.border),
		.unnormalizedCoordinates = vk::False,
	};
	auto [result, vk_sampler] = raii_device().createSampler(info);
	assert(result == vk::Result::eSuccess, "failed to create sampler: {}", vk::to_string(result));
	return adopt<gpu::handle<gpu::sampler>>(std::move(vk_sampler));
}

auto gse::vulkan::device::descriptor_heap_properties() const -> gpu::descriptor_heap_properties {
	return m_descriptor_heap_props;
}

auto gse::vulkan::device::create_descriptor_heap(const gpu::device_size size) -> gpu::handle<gpu::descriptor_heap> {
	const vk::BufferCreateInfo buffer_info{
		.size = size,
		.usage = vk::BufferUsageFlagBits::eDescriptorHeapEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};
	auto [buffer_result, vk_buffer] = raii_device().createBuffer(buffer_info);
	assert(buffer_result == vk::Result::eSuccess, "failed to create descriptor heap buffer: {}", vk::to_string(buffer_result));

	const auto reqs = (*m_device).getBufferMemoryRequirements(*vk_buffer);
	const auto mem_props = std::bit_cast<vk::PhysicalDevice>(m_physical_device.handle()).getMemoryProperties();
	std::uint32_t memory_type_index = std::numeric_limits<std::uint32_t>::max();
	constexpr auto required = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	constexpr auto preferred = required | vk::MemoryPropertyFlagBits::eDeviceLocal;
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & preferred) == preferred) {
			memory_type_index = i;
			break;
		}
	}
	if (memory_type_index == std::numeric_limits<std::uint32_t>::max()) {
		for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
			if ((reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & required) == required) {
				memory_type_index = i;
				break;
			}
		}
	}
	assert(memory_type_index != std::numeric_limits<std::uint32_t>::max(), "no suitable memory type for descriptor heap");

	const vk::MemoryAllocateFlagsInfo flags_info{
		.flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
	};
	const vk::MemoryAllocateInfo alloc_info{
		.pNext = &flags_info,
		.allocationSize = reqs.size,
		.memoryTypeIndex = memory_type_index,
	};
	auto [memory_result, vk_memory] = raii_device().allocateMemory(alloc_info);
	assert(memory_result == vk::Result::eSuccess, "failed to allocate descriptor heap memory: {}", vk::to_string(memory_result));

	const auto bind_result = (*m_device).bindBufferMemory(*vk_buffer, *vk_memory, 0);
	assert(bind_result == vk::Result::eSuccess, "failed to bind descriptor heap memory: {}", vk::to_string(bind_result));
	auto [map_result, mapped_ptr] = (*m_device).mapMemory(*vk_memory, 0, size, {});
	assert(map_result == vk::Result::eSuccess, "failed to map descriptor heap memory: {}", vk::to_string(map_result));
	const auto address = (*m_device).getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = *vk_buffer });

	const auto handle = std::bit_cast<gpu::handle<gpu::descriptor_heap>>(*vk_buffer);
	std::lock_guard lock(m_mutex);
	m_owned.store(
		handle.value,
		descriptor_heap_resources{
			.buffer = std::move(vk_buffer),
			.memory = std::move(vk_memory),
			.address = address,
			.mapped = static_cast<std::byte*>(mapped_ptr),
		}
	);
	return handle;
}

auto gse::vulkan::device::descriptor_heap_address(const gpu::handle<gpu::descriptor_heap> heap) const -> gpu::device_address {
	const auto* resources = m_owned.find(heap);
	return resources ? resources->address : 0;
}

auto gse::vulkan::device::write_image_descriptor(const gpu::handle<gpu::descriptor_heap> heap, const gpu::device_size byte_offset, const gpu::image_descriptor_kind kind, const gpu::image& img) const -> void {
	const auto* resources = m_owned.find(heap);
	assert(resources, "write_image_descriptor: unknown descriptor heap");

	const auto& view = img.view_create_info();
	const auto aspects = view.aspects ? view.aspects : image_aspect_for(img.format());
	const vk::ImageViewCreateInfo view_info{
		.image = std::bit_cast<vk::Image>(img.handle()),
		.viewType = to_vk(view.view_type),
		.format = static_cast<vk::Format>(img.format()),
		.components = {},
		.subresourceRange = {
			.aspectMask = to_vk(aspects),
			.baseMipLevel = view.base_mip_level,
			.levelCount = view.level_count,
			.baseArrayLayer = view.base_array_layer,
			.layerCount = view.layer_count,
		},
	};
	const vk::ImageDescriptorInfoEXT image_info{
		.pView = &view_info,
		.layout = vk::ImageLayout::eGeneral,
	};
	const vk::ResourceDescriptorInfoEXT info{
		.type = kind == gpu::image_descriptor_kind::storage ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage,
		.data = {
			.pImage = &image_info,
		},
	};
	const vk::HostAddressRangeEXT dst{
		.address = resources->mapped + byte_offset,
		.size = static_cast<std::size_t>(m_descriptor_heap_props.image_descriptor_size),
	};
	const auto result = (*m_device).writeResourceDescriptorsEXT(1, &info, &dst);
	assert(result == vk::Result::eSuccess, "writeResourceDescriptorsEXT (image) failed: {}", static_cast<std::int32_t>(result));
}

auto gse::vulkan::device::write_buffer_descriptor(const gpu::handle<gpu::descriptor_heap> heap, const gpu::device_size byte_offset, const gpu::buffer_descriptor_kind kind, const gpu::device_address address, const gpu::device_size range) const -> void {
	const auto* resources = m_owned.find(heap);
	assert(resources, "write_buffer_descriptor: unknown descriptor heap");

	const vk::DeviceAddressRangeEXT address_range{
		.address = address,
		.size = range,
	};
	vk::DescriptorType type = vk::DescriptorType::eStorageBuffer;
	if (kind == gpu::buffer_descriptor_kind::uniform) {
		type = vk::DescriptorType::eUniformBuffer;
	}
	else if (kind == gpu::buffer_descriptor_kind::acceleration_structure) {
		type = vk::DescriptorType::eAccelerationStructureKHR;
	}
	const vk::ResourceDescriptorInfoEXT info{
		.type = type,
		.data = {
			.pAddressRange = &address_range,
		},
	};
	const vk::HostAddressRangeEXT dst{
		.address = resources->mapped + byte_offset,
		.size = static_cast<std::size_t>(m_descriptor_heap_props.buffer_descriptor_size),
	};
	const auto result = (*m_device).writeResourceDescriptorsEXT(1, &info, &dst);
	assert(result == vk::Result::eSuccess, "writeResourceDescriptorsEXT (buffer) failed: {}", static_cast<std::int32_t>(result));
}

auto gse::vulkan::device::write_sampler_descriptor(const gpu::handle<gpu::descriptor_heap> heap, const gpu::device_size byte_offset, const gpu::sampler_desc& desc) const -> void {
	const auto* resources = m_owned.find(heap);
	assert(resources, "write_sampler_descriptor: unknown descriptor heap");

	const vk::SamplerCreateInfo sampler_info{
		.magFilter = to_vk(desc.mag),
		.minFilter = to_vk(desc.min),
		.mipmapMode = desc.min == gpu::sampler_filter::nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear,
		.addressModeU = to_vk(desc.address_u),
		.addressModeV = to_vk(desc.address_v),
		.addressModeW = to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = to_vk(desc.border),
		.unnormalizedCoordinates = vk::False,
	};
	const vk::HostAddressRangeEXT dst{
		.address = resources->mapped + byte_offset,
		.size = static_cast<std::size_t>(m_descriptor_heap_props.sampler_descriptor_size),
	};
	const auto result = (*m_device).writeSamplerDescriptorsEXT(1, &sampler_info, &dst);
	assert(result == vk::Result::eSuccess, "writeSamplerDescriptorsEXT failed: {}", static_cast<std::int32_t>(result));
}

auto gse::vulkan::align_up(const gpu::device_size value, const gpu::device_size alignment) -> gpu::device_size {
	if (alignment == 0) {
		return value;
	}
	return (value + alignment - 1) / alignment * alignment;
}

auto gse::vulkan::device::init_bindless() -> void {
	constexpr std::uint32_t texture_capacity = 2048;
	constexpr std::uint32_t image_capacity = 65536;
	constexpr std::uint32_t buffer_capacity = 16384;
	constexpr std::uint32_t sampler_capacity = 512;

	m_descriptor_heap_props = query_descriptor_heap_props(m_physical_device);
	const auto& props = m_descriptor_heap_props;
	m_bindless = std::make_unique<bindless_state>();

	const auto resource_reserved = align_up(props.min_resource_heap_reserved_range, props.resource_heap_alignment);
	const auto texture_image_offset = resource_reserved;
	const auto image_range_offset = texture_image_offset + texture_capacity * props.image_descriptor_size;
	const auto buffer_range_offset = align_up(image_range_offset + image_capacity * props.image_descriptor_size, props.buffer_descriptor_alignment);
	const auto resource_heap_size = align_up(buffer_range_offset + buffer_capacity * props.buffer_descriptor_size, props.resource_heap_alignment);

	const auto sampler_reserved = align_up(props.min_sampler_heap_reserved_range, props.sampler_heap_alignment);
	const auto texture_sampler_offset = sampler_reserved;
	const auto sampler_range_offset = texture_sampler_offset + texture_capacity * props.sampler_descriptor_size;
	const auto sampler_heap_size = align_up(sampler_range_offset + sampler_capacity * props.sampler_descriptor_size, props.sampler_heap_alignment);

	m_bindless->resource_heap = create_descriptor_heap(resource_heap_size);
	m_bindless->sampler_heap = create_descriptor_heap(sampler_heap_size);

	m_bindless->layout = {
		.image_range_offset = image_range_offset,
		.image_stride = props.image_descriptor_size,
		.texture_image_offset = texture_image_offset,
		.buffer_range_offset = buffer_range_offset,
		.buffer_stride = props.buffer_descriptor_size,
		.texture_sampler_offset = texture_sampler_offset,
		.sampler_range_offset = sampler_range_offset,
		.sampler_stride = props.sampler_descriptor_size,
	};
	m_bindless->resource_binding = {
		.address = descriptor_heap_address(m_bindless->resource_heap),
		.size = resource_heap_size,
		.reserved_offset = 0,
		.reserved_size = resource_reserved,
	};
	m_bindless->sampler_binding = {
		.address = descriptor_heap_address(m_bindless->sampler_heap),
		.size = sampler_heap_size,
		.reserved_offset = 0,
		.reserved_size = sampler_reserved,
	};

	m_bindless->image_pool.base_offset = image_range_offset;
	m_bindless->image_pool.stride = props.image_descriptor_size;
	m_bindless->image_pool.reset(image_capacity);

	m_bindless->buffer_pool.base_offset = buffer_range_offset;
	m_bindless->buffer_pool.stride = props.buffer_descriptor_size;
	m_bindless->buffer_pool.reset(buffer_capacity);

	m_bindless->texture_pool.reset(texture_capacity);

	m_bindless->sampler_pool.base_offset = sampler_range_offset;
	m_bindless->sampler_pool.stride = props.sampler_descriptor_size;
	m_bindless->sampler_pool.reset(sampler_capacity);
}

auto gse::vulkan::device::allocate_buffer_slot() -> gpu::bindless_handle {
	return gpu::bindless_handle(&m_bindless->buffer_pool, m_bindless->buffer_pool.allocate());
}

auto gse::vulkan::device::allocate_image_slot() -> gpu::bindless_handle {
	return gpu::bindless_handle(&m_bindless->image_pool, m_bindless->image_pool.allocate());
}

auto gse::vulkan::device::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	write_buffer_descriptor(m_bindless->resource_heap, m_bindless->buffer_pool.offset(slot), gpu::buffer_descriptor_kind::storage, address, size);
}

auto gse::vulkan::device::write_uniform_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	write_buffer_descriptor(m_bindless->resource_heap, m_bindless->buffer_pool.offset(slot), gpu::buffer_descriptor_kind::uniform, address, size);
}

auto gse::vulkan::device::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	write_buffer_descriptor(m_bindless->resource_heap, m_bindless->buffer_pool.offset(slot), gpu::buffer_descriptor_kind::acceleration_structure, as_address, 0);
}

auto gse::vulkan::device::write_sampled_image(const gpu::bindless_slot slot, const gpu::image& img) -> void {
	write_image_descriptor(m_bindless->resource_heap, m_bindless->image_pool.offset(slot), gpu::image_descriptor_kind::sampled, img);
}

auto gse::vulkan::device::register_sampler(const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	const auto slot = m_bindless->sampler_pool.allocate();
	write_sampler_descriptor(m_bindless->sampler_heap, m_bindless->sampler_pool.offset(slot), desc);
	return gpu::bindless_handle(&m_bindless->sampler_pool, slot);
}

auto gse::vulkan::device::register_texture(const gpu::image& img, const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	const auto slot = m_bindless->texture_pool.allocate();
	const auto& layout = m_bindless->layout;
	write_image_descriptor(m_bindless->resource_heap, layout.texture_image_offset + slot.index * layout.image_stride, gpu::image_descriptor_kind::texture, img);
	write_sampler_descriptor(m_bindless->sampler_heap, layout.texture_sampler_offset + slot.index * layout.sampler_stride, desc);
	return gpu::bindless_handle(&m_bindless->texture_pool, slot);
}

auto gse::vulkan::device::bindless_layout() const -> gpu::bindless_layout {
	return m_bindless->layout;
}

auto gse::vulkan::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return m_bindless->resource_binding;
}

auto gse::vulkan::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return m_bindless->sampler_binding;
}

auto gse::vulkan::device::create_shader_program(const gpu::shader_program_create_info& info) -> gpu::shader_program {
	std::optional<vk::PushConstantRange> vk_pc_range;
	if (info.push_constant_range.has_value()) {
		vk_pc_range = to_vk(*info.push_constant_range);
	}
	const vk::PushConstantRange* pc_ptr = vk_pc_range.has_value() ? &*vk_pc_range : nullptr;
	const std::uint32_t pc_count = vk_pc_range.has_value() ? 1u : 0u;
	auto [layout_result, layout] = raii_device().createPipelineLayout({
		.pushConstantRangeCount = pc_count,
		.pPushConstantRanges = pc_ptr,
	});
	assert(layout_result == vk::Result::eSuccess, "failed to create pipeline layout: {}", vk::to_string(layout_result));
	const auto layout_handle = adopt<gpu::handle<gpu::pipeline_layout>>(std::move(layout));

	const auto bindless = build_bindless_mappings(info.bindings, bindless_layout(), info.push_offset_start);

	std::vector<gpu::stage_flag> stages;
	std::vector<gpu::handle<gpu::shader_object>> shader_handles;
	stages.reserve(info.stages.size());
	shader_handles.reserve(info.stages.size());

	if (info.stages.size() == 1) {
		std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT> subgroup_size_scratch;
		std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT> mapping_scratch;
		shader_spec_scratch spec_scratch;
		const auto vk_info = build_vk_shader_create_info(info.stages[0], bindless.mappings, subgroup_size_scratch, mapping_scratch, spec_scratch, {});
		auto [result, shaders] = raii_device().createShadersEXT(vk_info);
		assert(result == vk::Result::eSuccess, "failed to create shader: {}", vk::to_string(result));
		stages.push_back(info.stages[0].stage);
		shader_handles.push_back(adopt<gpu::handle<gpu::shader_object>>(std::move(shaders[0])));
	}
	else {
		std::vector<std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>> subgroup_size_scratch(info.stages.size());
		std::vector<std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>> mapping_scratch(info.stages.size());
		std::vector<shader_spec_scratch> spec_scratch(info.stages.size());
		std::vector<vk::ShaderCreateInfoEXT> vk_infos;
		vk_infos.reserve(info.stages.size());
		for (std::size_t i = 0; i < info.stages.size(); ++i) {
			vk_infos.push_back(build_vk_shader_create_info(info.stages[i], bindless.mappings, subgroup_size_scratch[i], mapping_scratch[i], spec_scratch[i], vk::ShaderCreateFlagBitsEXT::eLinkStage));
		}
		auto [result, shaders] = raii_device().createShadersEXT(vk_infos);
		assert(result == vk::Result::eSuccess, "failed to create linked shaders: {}", vk::to_string(result));
		for (std::size_t i = 0; i < shaders.size(); ++i) {
			stages.push_back(info.stages[i].stage);
			shader_handles.push_back(adopt<gpu::handle<gpu::shader_object>>(std::move(shaders[i])));
		}
	}

	return gpu::shader_program(
		layout_handle,
		std::move(stages),
		std::move(shader_handles),
		gpu::dynamic_pipeline_state{ info.state },
		info.is_compute,
		info.is_mesh
	);
}

auto gse::vulkan::device::create_blas(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> gpu::blas {
	const auto sizes = query_blas_build_sizes(geometry, prim_count);

	auto storage = create_buffer(
		gpu::buffer_desc{
			.size = sizes.acceleration_structure_size,
			.usage = gpu::buffer_flag::acceleration_structure_storage,
		}
	);

	const auto handle = create_acceleration_structure(
		storage.handle(),
		sizes.acceleration_structure_size,
		gpu::acceleration_structure_type::bottom_level
	);

	const auto address = acceleration_structure_address(handle);

	return gpu::blas{
		std::move(storage),
		handle,
		address,
	};
}

auto gse::vulkan::device::create_tlas(const std::uint32_t max_instances) -> gpu::tlas {
	const auto sizes = query_tlas_build_sizes(max_instances);

	auto storage = create_buffer(
		gpu::buffer_desc{
			.size = sizes.acceleration_structure_size,
			.usage = gpu::buffer_flag::acceleration_structure_storage,
		}
	);

	const auto alignment = acceleration_structure_scratch_alignment();
	auto scratch = create_buffer(
		gpu::buffer_desc{
			.size = std::max(sizes.build_scratch_size, sizes.update_scratch_size) +
				alignment,
			.usage = gpu::buffer_flag::storage,
		}
	);

	const gpu::device_size instance_buf_size = max_instances * sizeof(gpu::acceleration_structure_instance);
	auto instance_buf = create_buffer(
		gpu::buffer_desc{
			.size = instance_buf_size,
			.usage = gpu::buffer_flag::acceleration_structure_build_input | gpu::buffer_flag::storage,
		}
	);

	const auto handle = create_acceleration_structure(
		storage.handle(),
		sizes.acceleration_structure_size,
		gpu::acceleration_structure_type::top_level
	);

	const auto address = acceleration_structure_address(handle);

	return gpu::tlas{
		std::move(storage),
		std::move(scratch),
		std::move(instance_buf),
		handle,
		address,
	};
}
