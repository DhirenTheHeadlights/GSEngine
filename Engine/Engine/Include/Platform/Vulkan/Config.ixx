module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform.vulkan.objects;

import std;
export import vulkan_hpp;

export namespace gse::vulkan::config {
    namespace instance {
        vk::Instance instance;
        vk::SurfaceKHR surface;
    }

    namespace device {
        vk::PhysicalDevice physical_device;
        vk::Device device;
    }

    namespace queue {
        vk::Queue graphics;
        vk::Queue present;
    }

    namespace command {
        vk::CommandPool pool;
    }

    namespace descriptor {
        vk::DescriptorPool pool;
    }

    namespace sync {
        vk::Semaphore image_available_semaphore;
        vk::Semaphore render_finished_semaphore;
        vk::Fence in_flight_fence;
        std::uint32_t current_frame = 0;
    }

    struct swap_chain_details {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    namespace swap_chain {
        vk::SwapchainKHR swap_chain;
        vk::SurfaceFormatKHR surface_format;
        vk::PresentModeKHR present_mode;
        vk::Extent2D extent;
        std::vector<vk::Framebuffer> frame_buffers;
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> image_views;
        vk::Format format;
        swap_chain_details details;
    }

    vk::RenderPass render_pass;
}

export namespace gse::vulkan {
    struct queue_family {
        std::optional<std::uint32_t> graphics_family;
        std::optional<std::uint32_t> present_family;

        auto complete() const -> bool {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    auto get_memory_properties() -> vk::PhysicalDeviceMemoryProperties;
}

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
export VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif