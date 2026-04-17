export module gse.platform:vulkan_runtime;

import std;

import :vulkan_allocator;
import :descriptor_buffer_types;
import :gpu_image;

export namespace gse::vulkan {
	constexpr std::uint32_t max_frames_in_flight = 2;

	struct instance_config {
        vk::raii::Context context;
        vk::raii::Instance instance;
        vk::raii::SurfaceKHR surface;
        instance_config(vk::raii::Context&& context, vk::raii::Instance&& instance, vk::raii::SurfaceKHR&& surface)
            : context(std::move(context)), instance(std::move(instance)), surface(std::move(surface)) {
        }
        instance_config(instance_config&&) = default;
        auto operator=(instance_config&&) -> instance_config & = default;
    };

    struct device_config {
        vk::raii::PhysicalDevice physical_device;
        vk::raii::Device device;
        bool device_fault_enabled = false;
        bool device_fault_vendor_binary_enabled = false;
        device_config(
            vk::raii::PhysicalDevice&& physical_device,
            vk::raii::Device&& device,
            const bool device_fault_enabled = false,
            const bool device_fault_vendor_binary_enabled = false
        )
            : physical_device(std::move(physical_device)),
            device(std::move(device)),
            device_fault_enabled(device_fault_enabled),
            device_fault_vendor_binary_enabled(device_fault_vendor_binary_enabled) {
        }
        device_config(device_config&&) = default;
        auto operator=(device_config&&) -> device_config & = default;
    };

    struct queue_config {
        vk::raii::Queue graphics;
        vk::raii::Queue present;
        vk::raii::Queue compute;
        std::uint32_t compute_family_index = 0;
        std::unique_ptr<std::recursive_mutex> mutex;

        vk::raii::Queue video_encode{nullptr};
        std::optional<std::uint32_t> video_encode_family_index;

        queue_config(vk::raii::Queue&& graphics, vk::raii::Queue&& present, vk::raii::Queue&& compute, std::uint32_t compute_family)
            : graphics(std::move(graphics)), present(std::move(present)), compute(std::move(compute)), compute_family_index(compute_family), mutex(std::make_unique<std::recursive_mutex>()) {
        }
        queue_config(queue_config&&) = default;
        auto operator=(queue_config&&) -> queue_config & = default;

        auto set_video_encode(vk::raii::Queue&& q, std::uint32_t family) -> void {
            video_encode = std::move(q);
            video_encode_family_index = family;
        }
    };

    struct command_config {
        vk::raii::CommandPool pool;
        std::vector<vk::raii::CommandBuffer> buffers;
        std::unique_ptr<std::recursive_mutex> pool_mutex;
        std::uint32_t graphics_family = 0;
        command_config(vk::raii::CommandPool&& pool, std::vector<vk::raii::CommandBuffer>&& buffers, std::uint32_t graphics_family)
            : pool(std::move(pool)), buffers(std::move(buffers)), pool_mutex(std::make_unique<std::recursive_mutex>()), graphics_family(graphics_family) {
        }
        command_config(command_config&&) = default;
        auto operator=(command_config&&) -> command_config & = default;
    };

    struct transient_gpu_work {
        vk::raii::CommandBuffer command_buffer = nullptr;
        vk::raii::Fence fence = nullptr;
        std::vector<buffer_resource> transient_buffers;
    };

    struct sync_config {
        std::vector<vk::raii::Semaphore> image_available_semaphores;
		std::vector<vk::raii::Semaphore> render_finished_semaphores;
        std::vector<vk::raii::Fence> in_flight_fences;
        std::uint32_t timeline_value = 0;
        sync_config(
            std::vector<vk::raii::Semaphore>&& image_available_semaphores,
			std::vector<vk::raii::Semaphore>&& render_finished_semaphores,
            std::vector<vk::raii::Fence>&& in_flight_fences)
            : image_available_semaphores(std::move(image_available_semaphores)),
            render_finished_semaphores(std::move(render_finished_semaphores)),
    		in_flight_fences(std::move(in_flight_fences)) {
        }
        sync_config(sync_config&&) = default;
        auto operator=(sync_config&&) -> sync_config & = default;
    };

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
            present_modes(std::move(present_modes)) {
        }
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
        gpu::image depth_image;

        swap_chain_config(
            vk::raii::SwapchainKHR&& swap_chain,
            const vk::SurfaceFormatKHR surface_format,
            const vk::PresentModeKHR present_mode,
            const vk::Extent2D extent,
            std::vector<vk::Image>&& images,
            std::vector<vk::raii::ImageView>&& image_views,
            const vk::Format format,
            swap_chain_details&& details,
            gpu::image&& depth_image
        )
            : swap_chain(std::move(swap_chain)),
            surface_format(surface_format),
            present_mode(present_mode),
            extent(extent),
            images(std::move(images)),
            image_views(std::move(image_views)),
            format(format),
            details(std::move(details)),
            depth_image(std::move(depth_image)) {
        }

        swap_chain_config(swap_chain_config&&) = default;
        auto operator=(swap_chain_config&&) -> swap_chain_config & = default;
    };

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
        auto operator=(frame_context_config&&) -> frame_context_config & = default;
    };

    struct queue_family {
        std::optional<std::uint32_t> graphics_family;
        std::optional<std::uint32_t> present_family;
        std::optional<std::uint32_t> compute_family;
        std::optional<std::uint32_t> video_encode_family;

        auto complete() const -> bool {
			return graphics_family.has_value() && present_family.has_value() && compute_family.has_value();
        }
    };
}

