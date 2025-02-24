module;

#include <algorithm>
#include <vulkan/vulkan_hpp_macros.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <optional>
#include <set>
#include <vector>

export module gse.platform.context;

import vulkan_hpp;

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
export VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
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

struct swap_chain_config {
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;

    swap_chain_details details;
};

vk::Instance g_instance;
vk::PhysicalDevice g_physical_device;
vk::Device g_device;
vk::Queue g_graphics_queue;
vk::Queue g_present_queue;
vk::SurfaceKHR g_surface;
vk::CommandPool g_command_pool;
vk::DescriptorPool g_descriptor_pool;

export namespace gse::vulkan {
    auto initialize(GLFWwindow* window) -> void;
    auto get_swap_chain(GLFWwindow* window) -> swap_chain_config;
    auto get_queue_families() -> queue_family;

	auto get_instance() -> vk::Instance {
		return g_instance;
	}

    auto get_physical_device() -> vk::PhysicalDevice {
        return g_physical_device;
    }

    auto get_device() -> vk::Device {
        return g_device;
    }

    auto get_graphics_queue() -> vk::Queue {
        return g_graphics_queue;
    }

    auto get_present_queue() -> vk::Queue {
        return g_present_queue;
    }

    auto get_surface() -> vk::SurfaceKHR {
        return g_surface;
    }

	auto get_command_pool() -> vk::CommandPool {
		return g_command_pool;
	}

	auto get_descriptor_pool() -> vk::DescriptorPool {
		return g_descriptor_pool;
	}

    auto find_memory_type(const std::uint32_t type_filter, const vk::MemoryPropertyFlags properties) -> std::uint32_t;

	auto begin_single_line_commands() -> vk::CommandBuffer;
	auto end_single_line_commands(vk::CommandBuffer command_buffer) -> void;

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
}

auto gse::vulkan::initialize(GLFWwindow* window) -> void {
    create_instance(window);
    select_gpu();
    create_logical_device(g_surface);
}

auto gse::vulkan::get_swap_chain(GLFWwindow* window) -> swap_chain_config {
    const auto swap_chain_details = query_swap_chain_support(g_physical_device, g_surface);

    return {
        choose_swap_chain_surface_format(swap_chain_details.formats),
        choose_swap_chain_present_mode(swap_chain_details.present_modes),
        choose_swap_chain_extent(swap_chain_details.capabilities, window),
        swap_chain_details
    };
}

auto gse::vulkan::get_queue_families() -> queue_family {
    return find_queue_families(g_physical_device, g_surface);
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

    constexpr uint32_t min_supported_version = vk::makeApiVersion(1, 1, 0, 0);
    const uint32_t selected_version = std::max(min_supported_version, highest_supported_version);

    const vk::ApplicationInfo app_info(
        "My Vulkan Renderer", 1,                // App name and version
        "No Engine", 1,                         // Engine name and version
        selected_version                        // Vulkan API version
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
    assert_comment(glfwCreateWindowSurface(g_instance, window, nullptr, &surface) == static_cast<int>(vk::Result::eSuccess), "Error creating window surface");
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

    g_device = g_physical_device.createDevice(create_info);
    g_graphics_queue = g_device.getQueue(graphics_family.value(), 0);
    g_present_queue = g_device.getQueue(present_family.value(), 0);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
    VULKAN_HPP_DEFAULT_DISPATCHER.init(g_device);
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

auto gse::vulkan::shutdown() -> void {
	g_device.destroyCommandPool(g_command_pool);
    g_instance.destroySurfaceKHR(g_surface);
    g_instance.destroy();
    g_device.destroy();
}

