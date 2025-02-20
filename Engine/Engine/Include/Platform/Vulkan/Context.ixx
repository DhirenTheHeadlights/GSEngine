module;

#include <vulkan/vulkan_hpp_macros.hpp>

#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <optional>
#include <set>

export module gse.platform.vulkan.context;

import vulkan_hpp;

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

struct queue_family {
	std::optional<std::uint32_t> graphics_family;
	std::optional<std::uint32_t> present_family;

	auto complete() const -> bool {
		return graphics_family.has_value() && present_family.has_value();
	}
};

vk::Instance g_instance;
vk::PhysicalDevice g_physical_device;
vk::Device g_device;
vk::Queue g_graphics_queue;
vk::Queue g_present_queue;

export namespace gse::vulkan {
	auto create_instance() -> void;
	auto select_gpu() -> void;
	auto find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> queue_family;
	auto create_logical_device(vk::SurfaceKHR surface) -> void;
	auto destroy_instance() -> void;

	auto get_instance() -> vk::Instance& {
		return g_instance;
	}
}

auto gse::vulkan::create_instance() -> void {
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

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
	VULKAN_HPP_DEFAULT_DISPATCHER.init(g_instance);
#endif
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

    vk::PhysicalDeviceFeatures device_features{};

    vk::DeviceCreateInfo create_info(
        {}, queue_create_infos.size(), queue_create_infos.data(),
        0, nullptr, 0, nullptr, &device_features
    );

    g_device = g_physical_device.createDevice(create_info);
    g_graphics_queue = g_device.getQueue(graphics_family.value(), 0);
    g_present_queue = g_device.getQueue(present_family.value(), 0);

    std::cout << "Logical Device Created Successfully!\n";
}

auto gse::vulkan::destroy_instance() -> void {
	g_instance.destroy();
    g_device.destroy();
}

