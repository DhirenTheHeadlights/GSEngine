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

struct queue_family {
    std::optional<std::uint32_t> graphics_family;
    std::optional<std::uint32_t> present_family;

    auto complete() const -> bool {
        return graphics_family.has_value() && present_family.has_value();
    }
};

vk::DebugUtilsMessengerEXT g_debug_utils_messenger;

constexpr std::uint32_t max_frames_in_flight = 2;

export namespace gse::vulkan {
    auto initialize(GLFWwindow* window) -> void;

    auto get_next_image(GLFWwindow* window) -> std::uint32_t;
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


