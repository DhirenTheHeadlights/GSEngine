module;

#include <algorithm>
#include <vulkan/vulkan_hpp_macros.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <vector>

export module gse.platform.context;

import vulkan_hpp;

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
export VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

import gse.platform.assert;

struct queue_family {
    std::optional<std::uint32_t> graphics_family;
    std::optional<std::uint32_t> present_family;

    auto complete() const -> bool {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct instance_config {
    vk::Instance instance;
    vk::SurfaceKHR surface;
};

struct device_config {
    vk::PhysicalDevice physical_device;
    vk::Device device;
};

struct queue_config {
    vk::Queue graphics_queue;
    vk::Queue present_queue;
};

struct command_config {
    vk::CommandPool command_pool;
};

struct descriptor_config {
	vk::DescriptorPool descriptor_pool;
};

struct sync_objects {
    vk::Semaphore image_available_semaphore;
    vk::Semaphore render_finished_semaphore;
    vk::Fence in_flight_fence;
};

struct swap_chain_details {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

struct swap_chain_config {
    vk::SwapchainKHR swap_chain;
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;
	std::vector<vk::Framebuffer> frame_buffers;
	std::vector<vk::Image> images;
	std::vector<vk::ImageView> image_views;
    vk::Format format;

    swap_chain_details details;
};

instance_config     g_instance_config;
device_config       g_device_config;
queue_config        g_queue_config;
command_config      g_command_config;
descriptor_config   g_descriptor_config;
sync_objects        g_sync_objects;
swap_chain_config   g_swap_chain_config;

vk::RenderPass g_render_pass;

vk::DebugUtilsMessengerEXT g_debug_utils_messenger;

std::uint32_t g_current_frame = 0;
constexpr std::uint32_t max_frames_in_flight = 2;

export namespace gse::vulkan {
    auto initialize(GLFWwindow* window) -> void;
    auto get_queue_families() -> queue_family;

	auto get_swap_chain_config()                    -> swap_chain_config&;
	auto get_instance_config()                      -> instance_config&;
	auto get_device_config()                        -> device_config&;
	auto get_queue_config()                         -> queue_config&;
	auto get_command_config()                       -> command_config&;
	auto get_descriptor_config()                    -> descriptor_config&;
	auto get_sync_objects()                         -> sync_objects&;

    auto get_next_image(GLFWwindow* window) -> std::uint32_t;
	auto get_current_frame() -> std::uint32_t;
    auto find_memory_type(std::uint32_t type_filter, vk::MemoryPropertyFlags properties) -> std::uint32_t;

	auto begin_single_line_commands() -> vk::CommandBuffer;
	auto end_single_line_commands(vk::CommandBuffer command_buffer) -> void;
	auto create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& buffer_memory) -> vk::Buffer;

    auto shutdown() -> void;
}

namespace gse::vulkan {
    auto create_instance(GLFWwindow* window) -> void;
    auto select_gpu() -> void;
    auto find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> queue_family;
    auto create_logical_device(vk::SurfaceKHR surface) -> void;
    auto query_swap_chain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> swap_chain_details;
    auto choose_swap_chain_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) -> vk::SurfaceFormatKHR;
    auto choose_swap_chain_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) -> vk::PresentModeKHR;
    auto choose_swap_chain_extent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) -> vk::Extent2D;
	auto create_swap_chain(GLFWwindow* window) -> void;
	auto create_image_views() -> void;
	auto create_sync_objects() -> void;
    auto create_descriptors() -> void;
	auto create_command_pool() -> void;
}

auto debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity, vk::DebugUtilsMessageTypeFlagsEXT message_type, const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) -> vk::Bool32 {
	std::cerr << "Validation layer: " << callback_data->pMessage << "\n";
	return vk::False;
}

auto gse::vulkan::create_instance(GLFWwindow* window) -> void {
    const std::vector validation_layers = {
           "VK_LAYER_KHRONOS_validation"
    };

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

    const uint32_t highest_supported_version = vk::enumerateInstanceVersion();

    const uint32_t major = vk::apiVersionMajor(highest_supported_version);
    const uint32_t minor = vk::apiVersionMinor(highest_supported_version);
    const uint32_t patch = vk::apiVersionPatch(highest_supported_version);

    std::cout << "Running Vulkan " << major << "." << minor << "." << patch << "\n";

    constexpr uint32_t min_supported_version = vk::makeApiVersion(0, 1, 0, 0);
    const uint32_t selected_version = std::max(min_supported_version, highest_supported_version);

    const vk::ApplicationInfo app_info(
        "GSEngine", 1,
        "GSEngine", 1,
        selected_version
    );

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info(
        {},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debug_callback)
	);

    constexpr vk::ValidationFeatureEnableEXT enables[] = {
		vk::ValidationFeatureEnableEXT::eBestPractices,
		vk::ValidationFeatureEnableEXT::eGpuAssisted,
		vk::ValidationFeatureEnableEXT::eDebugPrintf
	};

	const vk::ValidationFeaturesEXT features(
		std::size(enables),
		enables,
		0, nullptr,
        &debug_create_info
	);

    const vk::InstanceCreateInfo create_info(
        {},
        &app_info,
        static_cast<uint32_t>(validation_layers.size()),
        validation_layers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data(),
		&features
    );

	auto& instance = g_instance_config.instance;
	auto& surface = g_instance_config.surface;

    try {
        instance = createInstance(create_info);
        std::cout << "Vulkan Instance Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Vulkan instance: " << err.what() << "\n";
    }

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#endif

    try {
        g_debug_utils_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
        std::cout << "Debug Messenger Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Debug Messenger: " << err.what() << "\n";
    }

    VkSurfaceKHR temp_surface;
    assert_comment(glfwCreateWindowSurface(instance, window, nullptr, &temp_surface) == static_cast<int>(vk::Result::eSuccess), "Error creating window surface");
	surface = temp_surface;
}

auto gse::vulkan::select_gpu() -> void {
	const auto devices = g_instance_config.instance.enumeratePhysicalDevices();
	auto& physical_device = g_device_config.physical_device;

	perma_assert(!devices.empty(), "No Vulkan-compatible GPUs found!");
    std::cout << "Found " << devices.size() << " Vulkan-compatible GPU(s):\n";

    for (const auto& device : devices) {
        vk::PhysicalDeviceProperties properties = device.getProperties();
        std::cout << " - " << properties.deviceName << " (Type: " << to_string(properties.deviceType) << ")\n";
    }

    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            physical_device = device;
            break;
        }
    }

    if (!physical_device) {
        physical_device = devices[0];
    }

    std::cout << "Selected GPU: " << physical_device.getProperties().deviceName << "\n";
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
	auto& [physical_device, device] = g_device_config;
	auto& [g_graphics_queue, g_present_queue] = g_queue_config;
    auto [graphics_family, present_family] = find_queue_families(physical_device, surface);

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

    vk::PhysicalDeviceFeatures supported_features = physical_device.getFeatures();

    vk::PhysicalDeviceFeatures device_features{};
    if (supported_features.samplerAnisotropy) {
        device_features.samplerAnisotropy = vk::True;
    }
    else {
        std::cerr << "Warning: samplerAnisotropy not supported by selected GPU.\n";
    }

    const std::vector device_extensions = {
        vk::KHRSwapchainExtensionName
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

    device = physical_device.createDevice(create_info);
    g_graphics_queue = device.getQueue(graphics_family.value(), 0);
    g_present_queue = device.getQueue(present_family.value(), 0);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#endif

    std::cout << "Logical Device Created Successfully!\n";
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
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
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

auto gse::vulkan::create_swap_chain(GLFWwindow* window) -> void {
    auto& [instance, surface] = g_instance_config;
    auto& [physical_device, device] = g_device_config;
    auto& swap_chain_details = g_swap_chain_config.details;

    swap_chain_details = query_swap_chain_support(physical_device, surface);

	auto& [swap_chain, surface_format, present_mode, swap_chain_extent, swap_chain_frame_buffers, swap_chain_images, swap_chain_image_views, swap_chain_image_format, details] = g_swap_chain_config;

    surface_format = choose_swap_chain_surface_format(swap_chain_details.formats);
    present_mode = choose_swap_chain_present_mode(swap_chain_details.present_modes);
	swap_chain_extent = choose_swap_chain_extent(swap_chain_details.capabilities, window);

    std::uint32_t image_count = swap_chain_details.capabilities.minImageCount + 1;
    if (swap_chain_details.capabilities.maxImageCount > 0 && image_count > swap_chain_details.capabilities.maxImageCount) {
        image_count = swap_chain_details.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info(
        {},
        surface,
        image_count,
        surface_format.format,
        surface_format.colorSpace,
        swap_chain_extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment
    );

    auto [graphics_family, present_family] = get_queue_families();
    const std::uint32_t queue_family_indices[] = { graphics_family.value(), present_family.value() };

    if (graphics_family != present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = swap_chain_details.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = true;

    swap_chain = device.createSwapchainKHR(create_info);

    swap_chain_images = device.getSwapchainImagesKHR(swap_chain);
    swap_chain_image_format = surface_format.format;

	swap_chain_frame_buffers.resize(swap_chain_images.size());

    for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
        vk::ImageView attachments[] = {
            swap_chain_image_views[i]
        };

        vk::FramebufferCreateInfo framebuffer_info(
            {},
            g_render_pass,
            static_cast<uint32_t>(std::size(attachments)),
            attachments,
			swap_chain_extent.width,
			swap_chain_extent.height,
            1
        );

        swap_chain_frame_buffers[i] = device.createFramebuffer(framebuffer_info);
    }

    std::cout << "Swapchain Created Successfully!\n";
}

auto gse::vulkan::create_image_views() -> void {
	const auto& swap_chain_images = g_swap_chain_config.images;
    const auto& swap_chain_image_format = g_swap_chain_config.format;
	auto& swap_chain_image_views = g_swap_chain_config.image_views;

    swap_chain_image_views.resize(swap_chain_images.size());

    for (size_t i = 0; i < swap_chain_images.size(); i++) {
        vk::ImageViewCreateInfo image_create_info(
            {},
            swap_chain_images[i],
            vk::ImageViewType::e2D,
            swap_chain_image_format,
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

        swap_chain_image_views[i] = g_device_config.device.createImageView(image_create_info);
    }

    std::cout << "Image Views Created Successfully!\n";
}

auto gse::vulkan::create_sync_objects() -> void {
	constexpr vk::SemaphoreCreateInfo semaphore_info;
	constexpr vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);

	const auto& device = g_device_config.device;
	auto& [image_available_semaphore, render_finished_semaphore, in_flight_fence] = g_sync_objects;

    image_available_semaphore = device.createSemaphore(semaphore_info);
    render_finished_semaphore = device.createSemaphore(semaphore_info);
    in_flight_fence = device.createFence(fence_info);

	std::cout << "Sync Objects Created Successfully!\n";
}

auto gse::vulkan::create_descriptors() -> void {
    constexpr vk::DescriptorPoolSize pool_size(
        vk::DescriptorType::eCombinedImageSampler, 100  // Support up to 100 textures
    );

    const vk::DescriptorPoolCreateInfo pool_info({}, 100, 1, &pool_size);
    g_descriptor_config.descriptor_pool = g_device_config.device.createDescriptorPool(pool_info);

	std::cout << "Descriptor Set Created Successfully!\n";
}

auto gse::vulkan::create_command_pool() -> void {
	const auto& [physical_device, device] = g_device_config;
	const auto [graphics_family, present_family] = find_queue_families(physical_device, g_instance_config.surface);

	const vk::CommandPoolCreateInfo pool_info(
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		graphics_family.value()
	);

	g_command_config.command_pool = device.createCommandPool(pool_info);

	std::cout << "Command Pool Created Successfully!\n";
}

auto gse::vulkan::shutdown() -> void {
    const auto device = get_device_config().device;

    device.waitIdle();
    device.destroyDescriptorPool(g_descriptor_config.descriptor_pool);

    device.destroySemaphore(g_sync_objects.image_available_semaphore);
    device.destroySemaphore(g_sync_objects.render_finished_semaphore);
    device.destroyFence(g_sync_objects.in_flight_fence);

    for (const auto& image_view : g_swap_chain_config.image_views) {
        device.destroyImageView(image_view);
    }

    device.destroySwapchainKHR(g_swap_chain_config.swap_chain);
    device.destroy();

    g_instance_config.instance.destroySurfaceKHR(g_instance_config.surface);
    g_instance_config.instance.destroy();
}

