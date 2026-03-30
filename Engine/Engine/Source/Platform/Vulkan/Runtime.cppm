module;

export module gse.platform:vulkan_runtime;

import std;

import :vulkan_allocator;
import :descriptor_heap;
import :descriptor_buffer_types;

import gse.assert;
import gse.log;

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
        queue_config(vk::raii::Queue&& graphics, vk::raii::Queue&& present, vk::raii::Queue&& compute, std::uint32_t compute_family)
            : graphics(std::move(graphics)), present(std::move(present)), compute(std::move(compute)), compute_family_index(compute_family), mutex(std::make_unique<std::recursive_mutex>()) {
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

    class runtime : non_copyable {
    public:
        runtime(
            instance_config&& instance_data, device_config&& device_data,
            queue_config&& queue, command_config&& command,
            sync_config&& sync,
            swap_chain_config&& swap_chain_data, frame_context_config&& frame_context,
            std::unique_ptr<allocator>&& alloc,
            std::unique_ptr<descriptor_heap>&& desc_heap,
            descriptor_buffer_properties&& desc_buf_props = {}
        ) : m_instance_data(std::move(instance_data)),
            m_device_data(std::move(device_data)),
            m_allocator(std::move(alloc)),
            m_queue(std::move(queue)),
            m_command(std::move(command)),
            m_descriptor_heap(std::move(desc_heap)),
            m_sync(std::move(sync)),
            m_swap_chain_data(std::move(swap_chain_data)),
            m_frame_context(std::move(frame_context)),
            m_descriptor_buffer_props(std::move(desc_buf_props)) {}

        using swap_chain_recreate_callback = std::function<void()>;

        auto on_swap_chain_recreate(swap_chain_recreate_callback callback) -> void {
            m_swap_chain_recreate_callbacks.push_back(std::move(callback));
        }

        auto notify_swap_chain_recreated() -> void {
            ++m_swap_chain_generation;
            for (const auto& callback : m_swap_chain_recreate_callbacks) {
                callback();
            }
        }

        auto report_device_lost(
            std::string_view operation
        ) -> void;

        auto swap_chain_generation() const -> std::uint64_t {
            return m_swap_chain_generation;
        }

        ~runtime() override {
            log::println(log::category::runtime, "Destroying Runtime");
        }

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

            try {
                m_queue.graphics.submit(submit_info, *fence);
            } catch (const vk::DeviceLostError&) {
                report_device_lost("transient graphics submission");
                throw;
            }

            m_sync.transient_work_graveyard[m_current_frame].push_back({
                .command_buffer = std::move(command_buffer),
                .fence = std::move(fence),
                .transient_buffers = std::move(transient_buffers)
            });
        }

        auto cleanup_finished_frame_resources() -> void {
            for (auto& work : m_sync.transient_work_graveyard[m_current_frame]) {
                if (*work.fence) {
                    try {
                        (void)m_device_data.device.waitForFences(
                            *work.fence,
                            vk::True,
                            std::numeric_limits<std::uint64_t>::max()
                        );
                    } catch (const vk::DeviceLostError&) {
                        report_device_lost("transient fence wait");
                        throw;
                    }
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

        [[nodiscard]] auto descriptor_heap(this auto& self) -> decltype(auto) {
            using self_t = std::remove_reference_t<decltype(self)>;
            using heap_ref_t = std::conditional_t<
                std::is_const_v<self_t>,
                const gse::vulkan::descriptor_heap&,
                gse::vulkan::descriptor_heap&
            >;
            assert(self.m_descriptor_heap.get(),
                std::source_location::current(),
                "Descriptor heap is unavailable because descriptor buffers are unsupported");
            return static_cast<heap_ref_t>(*self.m_descriptor_heap);
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

        auto set_mesh_shaders_enabled(const bool enabled) -> void {
	        m_mesh_shaders_enabled = enabled;
        }

        [[nodiscard]] auto mesh_shaders_enabled() const -> bool {
	        return m_mesh_shaders_enabled;
        }

        [[nodiscard]] auto descriptor_buffer_props() const -> const descriptor_buffer_properties& {
	        return m_descriptor_buffer_props;
        }
    private:
        struct instance_config m_instance_data;
        struct device_config m_device_data;
        std::unique_ptr<vulkan::allocator> m_allocator;
        struct queue_config m_queue;
        struct command_config m_command;
        std::unique_ptr<vulkan::descriptor_heap> m_descriptor_heap;
        struct sync_config m_sync;
        struct swap_chain_config m_swap_chain_data;
    	frame_context_config m_frame_context;
        std::uint32_t m_current_frame = 0;
        std::uint64_t m_swap_chain_generation = 0;
        bool m_frame_in_progress = false;
        bool m_mesh_shaders_enabled = false;
        descriptor_buffer_properties m_descriptor_buffer_props;
        std::atomic<bool> m_device_lost_reported = false;
        std::vector<swap_chain_recreate_callback> m_swap_chain_recreate_callbacks;
    };
}

auto gse::vulkan::runtime::report_device_lost(const std::string_view operation) -> void {
    bool expected = false;
    if (!m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    log::println(log::level::error, log::category::vulkan, "Vulkan device lost during {}", operation);

    if (!m_device_data.device_fault_enabled) {
        log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
        return;
    }

    vk::DeviceFaultCountsEXT counts{};
    if (const auto result = m_device_data.device.getFaultInfoEXT(&counts, nullptr); result != vk::Result::eSuccess) {
        log::println(log::level::warning, log::category::vulkan, "Failed to query device fault counts: {}", vk::to_string(result));
        return;
    }

    std::vector<vk::DeviceFaultAddressInfoEXT> address_infos(counts.addressInfoCount);
    std::vector<vk::DeviceFaultVendorInfoEXT> vendor_infos(counts.vendorInfoCount);
    std::vector<std::byte> vendor_binary(
        m_device_data.device_fault_vendor_binary_enabled ? static_cast<std::size_t>(counts.vendorBinarySize) : 0
    );

    counts.addressInfoCount = static_cast<std::uint32_t>(address_infos.size());
    counts.vendorInfoCount = static_cast<std::uint32_t>(vendor_infos.size());
    counts.vendorBinarySize = static_cast<vk::DeviceSize>(vendor_binary.size());

    vk::DeviceFaultInfoEXT fault_info{
        .pAddressInfos = address_infos.empty() ? nullptr : address_infos.data(),
        .pVendorInfos = vendor_infos.empty() ? nullptr : vendor_infos.data(),
        .pVendorBinaryData = vendor_binary.empty() ? nullptr : vendor_binary.data()
    };

    if (const auto result = m_device_data.device.getFaultInfoEXT(&counts, &fault_info); result != vk::Result::eSuccess) {
        log::println(log::level::warning, log::category::vulkan, "Failed to query device fault info: {}", vk::to_string(result));
        return;
    }

    const char* description = fault_info.description.data();
    log::println(
        log::level::error,
        log::category::vulkan,
        "Device fault description: {}",
        (description && description[0] != '\0') ? std::string_view(description) : std::string_view("(no description)")
    );

    for (std::size_t i = 0; i < address_infos.size(); ++i) {
        const auto& address_info = address_infos[i];
        log::println(
            log::level::error,
            log::category::vulkan,
            "Fault address {}: type={}, reported=0x{:x}, precision=0x{:x}",
            i,
            vk::to_string(address_info.addressType),
            address_info.reportedAddress,
            address_info.addressPrecision
        );
    }

    for (std::size_t i = 0; i < vendor_infos.size(); ++i) {
        const auto& vendor_info = vendor_infos[i];
        const char* vendor_description = vendor_info.description.data();
        log::println(
            log::level::error,
            log::category::vulkan,
            "Vendor fault {}: '{}' code=0x{:x} data=0x{:x}",
            i,
            (vendor_description && vendor_description[0] != '\0') ? std::string_view(vendor_description) : std::string_view("(no description)"),
            vendor_info.vendorFaultCode,
            vendor_info.vendorFaultData
        );
    }

    if (!vendor_binary.empty()) {
        log::println(log::level::error, log::category::vulkan, "Device fault vendor binary size: {} bytes", vendor_binary.size());
    }
}

export namespace gse::vulkan {
    struct queue_family {
        std::optional<std::uint32_t> graphics_family;
        std::optional<std::uint32_t> present_family;
        std::optional<std::uint32_t> compute_family;

        auto complete() const -> bool {
            return graphics_family.has_value() && present_family.has_value();
        }
    };
}

