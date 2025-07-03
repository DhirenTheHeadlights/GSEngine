module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform.vulkan:config;

import std;
export import vulkan_hpp;
import :resources;

export namespace gse::vulkan {
    struct config {
        struct instance_config {
            vk::raii::Context context;
            vk::raii::Instance instance;
            vk::raii::SurfaceKHR surface;
            instance_config(vk::raii::Context&& context, vk::raii::Instance&& instance, vk::raii::SurfaceKHR&& surface)
                : context(std::move(context)), instance(std::move(instance)), surface(std::move(surface)) {
            }
            instance_config(instance_config&&) = default;
            auto operator=(instance_config&&) -> instance_config& = default;
        } instance_data;

        struct device_config {
            vk::raii::PhysicalDevice physical_device;
            vk::raii::Device device;
            device_config(vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device)
                : physical_device(std::move(physical_device)), device(std::move(device)) {
            }
            device_config(device_config&&) = default;
            auto operator=(device_config&&) -> device_config& = default;
        } device_data;

        struct queue_config {
            vk::raii::Queue graphics;
            vk::raii::Queue present;
            queue_config(vk::raii::Queue&& graphics, vk::raii::Queue&& present)
                : graphics(std::move(graphics)), present(std::move(present)) {
            }
            queue_config(queue_config&&) = default;
            auto operator=(queue_config&&) -> queue_config& = default;
        } queue;

        struct command_config {
            vk::raii::CommandPool pool;
            std::vector<vk::raii::CommandBuffer> buffers;
            command_config(vk::raii::CommandPool&& pool, std::vector<vk::raii::CommandBuffer>&& buffers)
                : pool(std::move(pool)), buffers(std::move(buffers)) {
            }
            command_config(command_config&&) = default;
            auto operator=(command_config&&) -> command_config& = default;
        } command;

        struct descriptor_config {
            vk::raii::DescriptorPool pool;
            explicit descriptor_config(vk::raii::DescriptorPool&& pool)
                : pool(std::move(pool)) {
            }
            descriptor_config(descriptor_config&&) = default;
            auto operator=(descriptor_config&&) -> descriptor_config& = default;
        } descriptor;

        struct sync_config {
            std::vector<vk::raii::Semaphore> image_available_semaphores;
            std::vector<vk::raii::Semaphore> render_finished_semaphores;
            std::vector<vk::raii::Fence> in_flight_fences;
            std::vector<vk::Fence> images_in_flight;
            sync_config(
                std::vector<vk::raii::Semaphore>&& image_available_semaphores,
                std::vector<vk::raii::Semaphore>&& render_finished_semaphores,
                std::vector<vk::raii::Fence>&& in_flight_fences,
                std::vector<vk::Fence>&& images_in_flight)
                : image_available_semaphores(std::move(image_available_semaphores)),
                render_finished_semaphores(std::move(render_finished_semaphores)),
                in_flight_fences(std::move(in_flight_fences)),
                images_in_flight(std::move(images_in_flight)) {
            }
            sync_config(sync_config&&) = default;
            auto operator=(sync_config&&) -> sync_config& = default;
        } sync;

        struct swap_chain_details {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> present_modes;
            swap_chain_details(
                vk::SurfaceCapabilitiesKHR capabilities,
                std::vector<vk::SurfaceFormatKHR>&& formats,
                std::vector<vk::PresentModeKHR>&& present_modes
            ) : capabilities(std::move(capabilities)),
                formats(std::move(formats)),
                present_modes(std::move(present_modes)) {}
            swap_chain_details(swap_chain_details&&) = default;
            auto operator=(swap_chain_details&&) -> swap_chain_details& = default;
        };

        struct swap_chain_config {
            vk::raii::SwapchainKHR swap_chain;
            vk::SurfaceFormatKHR surface_format;
            vk::PresentModeKHR present_mode;
            vk::Extent2D extent;
            std::vector<vk::Image> images;
            std::vector<vk::raii::ImageView> image_views;
            vk::Format format;
            swap_chain_details details;
            persistent_allocator::image_resource normal_image;
            persistent_allocator::image_resource albedo_image;
            persistent_allocator::image_resource depth_image;

            swap_chain_config(
                vk::raii::SwapchainKHR&& swap_chain,
                const vk::SurfaceFormatKHR surface_format,
                const vk::PresentModeKHR present_mode,
                const vk::Extent2D extent,
                std::vector<vk::Image>&& images,
                std::vector<vk::raii::ImageView>&& image_views,
                const vk::Format format,
                swap_chain_details&& details,
                persistent_allocator::image_resource&& normal_image,
                persistent_allocator::image_resource&& albedo_image,
                persistent_allocator::image_resource&& depth_image
            )
                : swap_chain(std::move(swap_chain)),
                surface_format(surface_format),
                present_mode(present_mode),
                extent(extent),
                images(std::move(images)),
                image_views(std::move(image_views)),
                format(format),
                details(std::move(details)),
                normal_image(std::move(normal_image)),
                albedo_image(std::move(albedo_image)),
                depth_image(std::move(depth_image)) {
            }
            swap_chain_config(swap_chain_config&&) = default;
            auto operator=(swap_chain_config&&) -> swap_chain_config & = default;
        } swap_chain_data;

        struct frame_context_config {
            std::uint32_t image_index;
            vk::CommandBuffer command_buffer;
            frame_context_config(
                const std::uint32_t image_index,
                const vk::CommandBuffer command_buffer)
                : image_index(image_index),
                command_buffer(command_buffer) {
            }
            frame_context_config(frame_context_config&&) = default;
            auto operator=(frame_context_config&&) -> frame_context_config& = default;
        } frame_context;

        config(
            instance_config&& instance_data,
            device_config&& device_data,
            queue_config&& queue,
            command_config&& command,
            descriptor_config&& descriptor,
            sync_config&& sync,
            swap_chain_config&& swap_chain_data,
            frame_context_config&& frame_context)
            : instance_data(std::move(instance_data)),
            device_data(std::move(device_data)),
            queue(std::move(queue)),
            command(std::move(command)),
            descriptor(std::move(descriptor)),
            sync(std::move(sync)),
            swap_chain_data(std::move(swap_chain_data)),
            frame_context(std::move(frame_context)) {}
        config(config&&) = default;
        auto operator=(config&&) -> config& = default;
        ~config() {
            std::println("Destroying Config");
        }

        std::uint32_t current_frame = 0;
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

    auto get_memory_properties(vk::PhysicalDevice device) -> vk::PhysicalDeviceMemoryProperties;
}

#if defined(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC) && (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
export VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

auto gse::vulkan::get_memory_properties(const vk::PhysicalDevice device) -> vk::PhysicalDeviceMemoryProperties {
    return device.getMemoryProperties();
}