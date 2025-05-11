module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform.vulkan.config;

import std;
export import vulkan_hpp;

export namespace gse::vulkan {
    struct config {
        struct instance_data {
            vk::Instance instance;
            vk::SurfaceKHR surface;
        } instance_data;

        struct device_data {
            vk::PhysicalDevice physical_device;
            vk::Device device;
		} device_data;

        struct queue {
            vk::Queue graphics;
            vk::Queue present;
		} queue;

        struct command {
            vk::CommandPool pool;
            std::vector<vk::CommandBuffer> buffers;
		} command;

        struct descriptor {
            vk::DescriptorPool pool;
        } descriptor;

        struct sync {
            vk::Semaphore image_available_semaphore;
            vk::Semaphore render_finished_semaphore;
            vk::Fence in_flight_fence;
		} sync;

        struct swap_chain_details {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> present_modes;
        };

        struct swap_chain_data {
            vk::SwapchainKHR swap_chain;
            vk::SurfaceFormatKHR surface_format;
            vk::PresentModeKHR present_mode;
            vk::Extent2D extent;
            std::vector<vk::Framebuffer> frame_buffers;
            std::vector<vk::Image> images;
            std::vector<vk::ImageView> image_views;
            vk::Format format;
            swap_chain_details details;
        } swap_chain_data;

        struct frame_context {
            std::uint32_t image_index;
            vk::CommandBuffer command_buffer;
            vk::Framebuffer framebuffer;
		} frame_context;

        vk::RenderPass render_pass;
    };
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