module;

#include <algorithm>
#include <vulkan/vulkan_hpp_macros.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <optional>
#include <set>
#include <vector>

#include "vulkan/vulkan_core.h"

export module gse.platform.vulkan.context;

import vulkan_hpp;

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

import gse.platform.perma_assert;

struct queue_family {
	std::optional<std::uint32_t> graphics_family;
	std::optional<std::uint32_t> present_family;

	auto complete() const -> bool {
		return graphics_family.has_value() && present_family.has_value();
	}
};

struct swap_chain_details {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> present_modes;
};

vk::Instance g_instance;
vk::PhysicalDevice g_physical_device;
vk::Device g_device;
vk::Queue g_graphics_queue;
vk::Queue g_present_queue;
vk::SurfaceKHR g_surface;

vk::SwapchainKHR g_swap_chain;
std::vector<vk::Image> g_swap_chain_images;
vk::Format g_swap_chain_image_format;
vk::Extent2D g_swap_chain_extent;
std::vector<vk::ImageView> g_swap_chain_image_views;

vk::RenderPass g_render_pass;
std::vector<vk::Framebuffer> g_swap_chain_frame_buffers;

vk::CommandPool g_command_pool;
std::vector<vk::CommandBuffer> g_command_buffers;

export namespace gse::vulkan {
	auto initialize(GLFWwindow* window) -> void;
    auto shutdown() -> void;
}

namespace gse::vulkan {
    auto create_instance(GLFWwindow* window) -> void;
    auto select_gpu() -> void;
    auto find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> queue_family;
	auto create_logical_device(vk::SurfaceKHR surface) -> void;
    auto create_swap_chain(GLFWwindow* window) -> void;
    auto create_render_pass() -> void;
    auto create_frame_buffers() -> void;
    auto query_swap_chain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> swap_chain_details;
    auto choose_swap_chain_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) -> vk::SurfaceFormatKHR;
    auto choose_swap_chain_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) -> vk::PresentModeKHR;
	auto choose_swap_chain_extent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) -> vk::Extent2D;
	auto create_image_views() -> void;
    auto create_command_pool() -> void;
    auto create_command_buffers() -> void;
	auto destroy_swap_chain() -> void;
}

auto gse::vulkan::initialize(GLFWwindow* window) -> void {
	create_instance(window);
	select_gpu();
	create_logical_device(g_surface);
	create_swap_chain(window);
	create_render_pass();
	create_frame_buffers();
	create_command_pool();
	create_command_buffers();
}

auto gse::vulkan::create_instance(GLFWwindow* window) -> void {
    const std::vector validation_layers = {
           "VK_LAYER_KHRONOS_validation"
    };

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

    constexpr vk::ApplicationInfo app_info(
        "My Vulkan Renderer", 1,                // App name and version
        "No Engine", 1,                         // Engine name and version
		vk::makeApiVersion(1, 0, 0, 0)          // Vulkan version
    );

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    const vk::InstanceCreateInfo create_info(
        {},
        &app_info,
        static_cast<uint32_t>(validation_layers.size()),
        validation_layers.data(),
        glfw_extension_count,
        glfw_extensions
    );

    try {
        g_instance = createInstance(create_info);
        std::cout << "Vulkan Instance Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Vulkan instance: " << err.what() << "\n";
    }

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
	VULKAN_HPP_DEFAULT_DISPATCHER.init(g_instance);
#endif

    VkSurfaceKHR surface;
    assert_comment(glfwCreateWindowSurface(g_instance, window, nullptr, &surface) == VK_SUCCESS, "Error creating window surface");
	g_surface = surface;
}

auto gse::vulkan::select_gpu() -> void {
    const auto devices = g_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        std::cerr << "ERROR: No Vulkan-compatible GPUs found!\n";
        return;
    }

    std::cout << "Found " << devices.size() << " Vulkan-compatible GPU(s):\n";

    for (const auto& device : devices) {
        vk::PhysicalDeviceProperties properties = device.getProperties();
        std::cout << " - " << properties.deviceName << " (Type: " << to_string(properties.deviceType) << ")\n";
    }

    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            g_physical_device = device;
            break;
        }
    }

    if (!g_physical_device) {
        g_physical_device = devices[0];
    }

    std::cout << "Selected GPU: " << g_physical_device.getProperties().deviceName << "\n";
}

auto gse::vulkan::find_queue_families(const vk::PhysicalDevice device, const vk::SurfaceKHR surface) -> queue_family {
	queue_family indices;

	const auto queue_families = device.getQueueFamilyProperties();

	for (std::uint32_t i = 0; i < queue_families.size(); i++) {
		if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphics_family = i;
		}
		if (device.getSurfaceSupportKHR(i, surface)) {
			indices.present_family = i;
		}
		if (indices.complete()) {
			break;
		}
	}

	return indices;
}

auto gse::vulkan::create_logical_device(vk::SurfaceKHR surface) -> void {
    auto [graphics_family, present_family] = find_queue_families(g_physical_device, surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set unique_queue_families = {
        graphics_family.value(),
        present_family.value()
    };

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info({}, queue_family, 1, &queue_priority);
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures supported_features = g_physical_device.getFeatures();

    vk::PhysicalDeviceFeatures device_features{};
    if (supported_features.samplerAnisotropy) {
        device_features.samplerAnisotropy = VK_TRUE;
    }
    else {
        std::cerr << "Warning: samplerAnisotropy not supported by selected GPU.\n";
    }

    const std::vector device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    vk::DeviceCreateInfo create_info(
        {},
        static_cast<uint32_t>(queue_create_infos.size()),
        queue_create_infos.data(),
        0, nullptr,
        static_cast<uint32_t>(device_extensions.size()),
        device_extensions.data(),
        &device_features
    );

    g_device = g_physical_device.createDevice(create_info);
    g_graphics_queue = g_device.getQueue(graphics_family.value(), 0);
    g_present_queue = g_device.getQueue(present_family.value(), 0);

    std::cout << "Logical Device Created Successfully!\n";
}

auto gse::vulkan::create_swap_chain(GLFWwindow* window) -> void {
    const auto [capabilities, formats, present_modes] = query_swap_chain_support(g_physical_device, g_surface);

    const vk::SurfaceFormatKHR surface_format = choose_swap_chain_surface_format(formats);
    const vk::PresentModeKHR present_mode = choose_swap_chain_present_mode(present_modes);
    const vk::Extent2D extent = choose_swap_chain_extent(capabilities, window);

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info(
        {}, g_surface, image_count, surface_format.format, surface_format.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment
    );

    auto [graphics_family, present_family] = find_queue_families(g_physical_device, g_surface);
    const uint32_t queue_family_indices[] = { graphics_family.value(), present_family.value() };

    if (graphics_family != present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = true;

    g_swap_chain = g_device.createSwapchainKHR(create_info);

    g_swap_chain_images = g_device.getSwapchainImagesKHR(g_swap_chain);
    g_swap_chain_image_format = surface_format.format;
    g_swap_chain_extent = extent;

    create_image_views();

    std::cout << "Swapchain Created Successfully!\n";
}

auto gse::vulkan::create_render_pass() -> void {
    const vk::AttachmentDescription color_attachment(
        {}, g_swap_chain_image_format, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR
    );

    constexpr vk::AttachmentReference color_attachment_ref(
        0, vk::ImageLayout::eColorAttachmentOptimal
    );

    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const vk::SubpassDescription sub_pass(
        {}, vk::PipelineBindPoint::eGraphics,
        0, nullptr, 1, &color_attachment_ref
    );

    constexpr vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, vk::AccessFlagBits::eColorAttachmentWrite
    );

    const vk::RenderPassCreateInfo render_pass_info(
        {}, 1, &color_attachment, 1, &sub_pass, 1, &dependency
    );

    g_render_pass = g_device.createRenderPass(render_pass_info);

    std::cout << "Render Pass Created Successfully!\n";
}

auto gse::vulkan::create_frame_buffers() -> void {
	g_swap_chain_frame_buffers.resize(g_swap_chain_image_views.size());

    for (size_t i = 0; i < g_swap_chain_image_views.size(); i++) {
		std::array<vk::ImageView, 1> attachments = {
			g_swap_chain_image_views[i]
		};

        vk::FramebufferCreateInfo framebuffer_info(
            {}, g_render_pass, attachments.size(), attachments.data(),
            g_swap_chain_extent.width, g_swap_chain_extent.height, 1
        );

        g_swap_chain_frame_buffers[i] = g_device.createFramebuffer(framebuffer_info);
    }

    std::cout << "Framebuffers Created Successfully!\n";
}

auto gse::vulkan::query_swap_chain_support(const vk::PhysicalDevice device, const vk::SurfaceKHR surface) -> swap_chain_details {
	return {
		device.getSurfaceCapabilitiesKHR(surface),
		device.getSurfaceFormatsKHR(surface),
		device.getSurfacePresentModesKHR(surface)
    };
}

auto gse::vulkan::choose_swap_chain_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) -> vk::SurfaceFormatKHR {
	for (const auto& available_format : available_formats) {
		if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return available_format;
		}
	}
	return available_formats[0];
}

auto gse::vulkan::choose_swap_chain_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) -> vk::PresentModeKHR {
	for (const auto& available_present_mode : available_present_modes) {
		if (available_present_mode == vk::PresentModeKHR::eMailbox) {
			return available_present_mode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

auto gse::vulkan::choose_swap_chain_extent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) -> vk::Extent2D {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	vk::Extent2D actual_extent = {
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height)
	};

	actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	return actual_extent;
}

auto gse::vulkan::create_image_views() -> void {
    g_swap_chain_image_views.resize(g_swap_chain_images.size());

    for (size_t i = 0; i < g_swap_chain_images.size(); i++) {
        vk::ImageViewCreateInfo create_info(
            {},
            g_swap_chain_images[i],
            vk::ImageViewType::e2D,
            g_swap_chain_image_format,
            {
            	vk::ComponentSwizzle::eIdentity,
            	vk::ComponentSwizzle::eIdentity,
            	vk::ComponentSwizzle::eIdentity,
            	vk::ComponentSwizzle::eIdentity
            },
            {
            	vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
            }
        );

        g_swap_chain_image_views[i] = g_device.createImageView(create_info);
    }

    std::cout << "Image Views Created Successfully!\n";
}

auto gse::vulkan::create_command_pool() -> void {
	auto [graphics_family, present_family] = find_queue_families(g_physical_device, g_surface);

	const vk::CommandPoolCreateInfo pool_info(
		{}, graphics_family.value()
	);

	g_command_pool = g_device.createCommandPool(pool_info);
}

auto gse::vulkan::create_command_buffers() -> void {
    g_command_buffers.resize(g_swap_chain_frame_buffers.size());

    const vk::CommandBufferAllocateInfo alloc_info(
        g_command_pool, vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(g_command_buffers.size())
    );

    g_command_buffers = g_device.allocateCommandBuffers(alloc_info);

    std::cout << "Command Buffers Allocated Successfully!\n";
}

auto gse::vulkan::destroy_swap_chain() -> void {
    for (const auto framebuffer : g_swap_chain_frame_buffers) {
        g_device.destroyFramebuffer(framebuffer);
    }

    for (const auto image_view : g_swap_chain_image_views) {
        g_device.destroyImageView(image_view);
    }

    g_device.destroySwapchainKHR(g_swap_chain);
}

auto gse::vulkan::shutdown() -> void {
	g_device.destroyRenderPass(g_render_pass);
    destroy_swap_chain();
	g_instance.destroySurfaceKHR(g_surface);
	g_instance.destroy();
    g_device.destroy();
}

