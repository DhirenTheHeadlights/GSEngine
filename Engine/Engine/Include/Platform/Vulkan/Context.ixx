module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <compare>

export module gse.platform.vulkan.context;

import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.assert;

vk::DebugUtilsMessengerEXT g_debug_utils_messenger;

export namespace gse::vulkan {
	auto initialize(
        GLFWwindow* window
    ) -> config;

	auto begin_frame(
        GLFWwindow* window, 
        config& config
    ) -> void;

	auto end_frame(
        GLFWwindow* window, 
        config& config
    ) -> void;

	auto find_queue_families(
        const vk::raii::PhysicalDevice& device, 
        const vk::raii::SurfaceKHR& surface
    ) -> queue_family;

	auto begin_single_line_commands(
        const config& config
    ) -> vk::raii::CommandBuffer;

	auto end_single_line_commands(
		const vk::raii::CommandBuffer& command_buffer,
		const vulkan::config& config
	) -> void;

    auto shutdown(
        const config& config
    ) -> void;
}

namespace gse::vulkan {
    auto create_instance_and_surface(
        GLFWwindow* window
    ) -> config::instance_config;

    auto create_device_and_queues(
        const config::instance_config& instance_data
    ) -> std::tuple<config::device_config, config::queue_config>;

    auto create_command_objects(
        const config::device_config& device_data, 
        const config::instance_config& instance_data, 
        std::uint32_t image_count
    ) -> config::command_config;

    auto create_descriptor_pool(
        const config::device_config& device_data
    ) -> config::descriptor_config;

    auto create_sync_objects(
        const config::device_config& device_data, 
        std::uint32_t image_count
    ) -> config::sync_config;

    auto create_swap_chain_resources(
        GLFWwindow* window,
        const config::instance_config& instance_data,
        const config::device_config& device_data
    ) -> config::swap_chain_config;
}
