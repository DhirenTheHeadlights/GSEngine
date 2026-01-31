module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform.vulkan:config;

import std;

import :persistent_allocator;

export namespace gse::vulkan {
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
        device_config(vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device)
            : physical_device(std::move(physical_device)), device(std::move(device)) {
        }
        device_config(device_config&&) = default;
        auto operator=(device_config&&) -> device_config & = default;
    };

    struct queue_config {
        vk::raii::Queue graphics;
        vk::raii::Queue present;
        std::unique_ptr<std::recursive_mutex> mutex;
        queue_config(vk::raii::Queue&& graphics, vk::raii::Queue&& present)
            : graphics(std::move(graphics)), present(std::move(present)), mutex(std::make_unique<std::recursive_mutex>()) {
        }
        queue_config(queue_config&&) = default;
        auto operator=(queue_config&&) -> queue_config & = default;
    };

    struct command_config {
        vk::raii::CommandPool pool;
        std::vector<vk::raii::CommandBuffer> buffers;
        std::unique_ptr<std::recursive_mutex> pool_mutex;
        command_config(vk::raii::CommandPool&& pool, std::vector<vk::raii::CommandBuffer>&& buffers)
            : pool(std::move(pool)), buffers(std::move(buffers)), pool_mutex(std::make_unique<std::recursive_mutex>()) {
        }
        command_config(command_config&&) = default;
        auto operator=(command_config&&) -> command_config & = default;
    };

    struct descriptor_config {
        vk::raii::DescriptorPool pool;
    	std::unique_ptr<std::recursive_mutex> mutex;
        explicit descriptor_config(vk::raii::DescriptorPool&& pool)
            : pool(std::move(pool)), mutex(std::make_unique<std::recursive_mutex>()) {
        }
        descriptor_config(descriptor_config&&) = default;
        auto operator=(descriptor_config&&) -> descriptor_config & = default;
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
        std::vector<std::vector<transient_gpu_work>> transient_work_graveyard;
        std::uint32_t timeline_value = 0;
        sync_config(
            std::vector<vk::raii::Semaphore>&& image_available_semaphores,
			std::vector<vk::raii::Semaphore>&& render_finished_semaphores,
            std::vector<vk::raii::Fence>&& in_flight_fences)
            : image_available_semaphores(std::move(image_available_semaphores)),
            render_finished_semaphores(std::move(render_finished_semaphores)),
    		in_flight_fences(std::move(in_flight_fences)) {
            transient_work_graveyard.resize(this->in_flight_fences.size());
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
        image_resource normal_image;
        image_resource albedo_image;
        image_resource depth_image;

        swap_chain_config(
            vk::raii::SwapchainKHR&& swap_chain,
            const vk::SurfaceFormatKHR surface_format,
            const vk::PresentModeKHR present_mode,
            const vk::Extent2D extent,
            std::vector<vk::Image>&& images,
            std::vector<vk::raii::ImageView>&& image_views,
            const vk::Format format,
            swap_chain_details&& details,
            image_resource&& normal_image,
            image_resource&& albedo_image,
            image_resource&& depth_image
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

    class config {
    public:
        config(
            instance_config&& instance_data, device_config&& device_data,
            queue_config&& queue, command_config&& command,
            descriptor_config&& descriptor, sync_config&& sync,
            swap_chain_config&& swap_chain_data, frame_context_config&& frame_context,
            std::unique_ptr<allocator>&& alloc
        ) : m_allocator(std::move(alloc)),
            m_instance_data(std::move(instance_data)),
            m_device_data(std::move(device_data)),
            m_queue(std::move(queue)),
            m_command(std::move(command)),
            m_descriptor(std::move(descriptor)),
            m_sync(std::move(sync)),
            m_swap_chain_data(std::move(swap_chain_data)),
            m_frame_context(std::move(frame_context)) {}

        using swap_chain_recreate_callback = std::function<void(config&)>;

        auto on_swap_chain_recreate(swap_chain_recreate_callback callback) -> void {
            m_swap_chain_recreate_callbacks.push_back(std::move(callback));
        }

        auto notify_swap_chain_recreated() -> void {
            ++m_swap_chain_generation;
            for (const auto& callback : m_swap_chain_recreate_callbacks) {
                callback(*this);
            }
        }

        auto swap_chain_generation() const -> std::uint64_t {
            return m_swap_chain_generation;
        }

        ~config() {
            std::println("Destroying Config");
        }

        config(config&&) = default;
        auto operator=(config&&) -> config & = default;

        auto add_transient_work(const std::function<std::vector<buffer_resource>(const vk::raii::CommandBuffer&)>& commands) -> void {
            const vk::CommandBufferAllocateInfo alloc_info{
                .commandPool = *m_command.pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            };

            auto buffers = m_device_data.device.allocateCommandBuffers(alloc_info);
            vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

            constexpr vk::CommandBufferBeginInfo begin_info{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
            };

            command_buffer.begin(begin_info);
            auto transient_buffers = commands(command_buffer);
            command_buffer.end();

            vk::raii::Fence fence = m_device_data.device.createFence({});

            const vk::SubmitInfo submit_info{
                .commandBufferCount = 1,
                .pCommandBuffers = &*command_buffer
            };

            m_queue.graphics.submit(submit_info, *fence);

            m_sync.transient_work_graveyard[m_current_frame].push_back({
                .command_buffer = std::move(command_buffer),
                .fence = std::move(fence),
                .transient_buffers = std::move(transient_buffers)
            });
        }

        auto cleanup_finished_frame_resources() -> void {
            for (auto& work : m_sync.transient_work_graveyard[m_current_frame]) {
                if (*work.fence) {
                    (void)m_device_data.device.waitForFences(
                        *work.fence,
                        vk::True,
                        std::numeric_limits<std::uint64_t>::max()
                    );
                }
            }
            m_sync.transient_work_graveyard[m_current_frame].clear();
        }

        auto instance_config() -> instance_config& {
            return m_instance_data;
        }

        auto instance_config() const -> const struct instance_config& {
            return m_instance_data;
		}

        auto device_config() -> device_config& {
            return m_device_data;
		}

        auto device_config() const -> const struct device_config& {
            return m_device_data;
		}

        auto queue_config() -> queue_config& {
            return m_queue;
		}

        auto command_config() -> command_config& {
            return m_command;
		}

        auto descriptor_config() -> descriptor_config& {
            return m_descriptor;
        }

        auto sync_config() -> sync_config& {
            return m_sync;
        }

        auto swap_chain_config() -> swap_chain_config& {
			return m_swap_chain_data;
        }

		auto swap_chain_config() const -> const struct swap_chain_config& {
			return m_swap_chain_data;
		}

        auto frame_context() -> frame_context_config& {
            return m_frame_context;
		}

        auto current_frame() -> std::uint32_t& {
            return m_current_frame;
		}

        auto set_frame_in_progress(const bool in_progress) -> void {
            m_frame_in_progress = in_progress;
        }

        [[nodiscard]] auto frame_in_progress() const -> bool {
            return m_frame_in_progress;
        }

        [[nodiscard]] auto allocator() -> allocator& {
            return *m_allocator;
        }

        [[nodiscard]] auto allocator() const -> const vulkan::allocator& {
            return *m_allocator;
        }

    private:
        struct instance_config m_instance_data;
        struct device_config m_device_data;
        std::unique_ptr<vulkan::allocator> m_allocator;
        struct queue_config m_queue;
        struct command_config m_command;
        struct descriptor_config m_descriptor;
        struct sync_config m_sync;
        struct swap_chain_config m_swap_chain_data;
    	frame_context_config m_frame_context;
        std::uint32_t m_current_frame = 0;
        std::uint64_t m_swap_chain_generation = 0;
        bool m_frame_in_progress = false;
        std::vector<swap_chain_recreate_callback> m_swap_chain_recreate_callbacks;
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
}

