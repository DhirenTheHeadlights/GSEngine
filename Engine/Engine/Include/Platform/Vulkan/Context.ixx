module;

#include <algorithm>
#include <vulkan/vulkan_hpp_macros.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <optional>
#include <span>
#include <vector>

export module gse.platform.vulkan.context;

import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.assert;

vk::DebugUtilsMessengerEXT g_debug_utils_messenger;

constexpr std::uint32_t max_frames_in_flight = 2;

export namespace gse::vulkan {
    auto initialize(GLFWwindow* window) -> void;

    auto get_next_image(GLFWwindow* window) -> std::uint32_t;
    auto find_memory_type(std::uint32_t type_filter, vk::MemoryPropertyFlags properties) -> std::uint32_t;
	auto find_queue_families(vk::PhysicalDevice device = config::device::physical_device, vk::SurfaceKHR surface = config::instance::surface) -> queue_family;

	auto begin_single_line_commands() -> vk::CommandBuffer;
	auto end_single_line_commands(vk::CommandBuffer command_buffer) -> void;
	auto create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& buffer_memory) -> vk::Buffer;

    auto shutdown() -> void;
}

namespace gse::vulkan {
    auto create_instance(GLFWwindow* window) -> void;
    auto select_gpu() -> void;
    auto create_logical_device(vk::SurfaceKHR surface) -> void;
    auto query_swap_chain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> config::swap_chain_details;
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

    try {
        config::instance::instance = createInstance(create_info);
        std::cout << "Vulkan Instance Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Vulkan instance: " << err.what() << "\n";
    }

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(config::instance::instance);
#endif

    try {
        g_debug_utils_messenger = config::instance::instance.createDebugUtilsMessengerEXT(debug_create_info);
        std::cout << "Debug Messenger Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Debug Messenger: " << err.what() << "\n";
    }

    VkSurfaceKHR temp_surface;
    assert(glfwCreateWindowSurface(config::instance::instance, window, nullptr, &temp_surface) == static_cast<int>(vk::Result::eSuccess), "Error creating window surface");
    config::instance::surface = temp_surface;
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



