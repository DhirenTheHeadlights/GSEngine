module;

#include <vulkan/vulkan_hpp_macros.hpp>

#include <iostream>
#include <vector>

export module gse.platform.vulkan.context;

import vulkan_hpp;

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

vk::Instance g_instance;
vk::PhysicalDevice g_physical_device;

export namespace gse::vulkan {
	auto initialize() -> void;
	auto exit() -> void;
}

namespace gse::vulkan {
	auto create_instance() -> void;
	auto select_gpu() -> void;
	auto destroy_instance() -> void;
}

auto gse::vulkan::initialize() -> void {
	create_instance();
}

auto gse::vulkan::exit() -> void {
	destroy_instance();
}

auto gse::vulkan::create_instance() -> void {
    const std::vector validation_layers = {
           "VK_LAYER_KHRONOS_validation"
    };

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

    constexpr vk::ApplicationInfo app_info(
        "My Vulkan Renderer", 1,            // App name and version
        "No Engine", 1,                     // Engine name and version
		vk::makeApiVersion(1, 0, 0, 0)       // Vulkan version
    );

    const vk::InstanceCreateInfo create_info(
        {},                         // Flags
        &app_info,                  // Application info
        validation_layers.size(),   // Enabled validation layers
        validation_layers.data(),   // Validation layer names
        0, nullptr                  // Extensions (we'll add later)
    );

    try {
        g_instance = createInstance(create_info);
        std::cout << "Vulkan Instance Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Vulkan instance: " << err.what() << "\n";
    }
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

    // Pick the first discrete GPU, fallback to first available device
    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            g_physical_device = device;
            break;
        }
    }

    if (!g_physical_device) {
        g_physical_device = devices[0]; // Pick the first GPU if no discrete GPU is found
    }

    std::cout << "Selected GPU: " << g_physical_device.getProperties().deviceName << "\n";
}

auto gse::vulkan::destroy_instance() -> void {
	g_instance.destroy();
}

