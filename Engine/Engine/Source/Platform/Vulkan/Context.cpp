module;

#include <compare>

module gse.platform.context;

import std;
import vulkan_hpp;

import gse.platform.assert;

auto gse::vulkan::initialize(GLFWwindow* window) -> void {
    create_instance(window);
    select_gpu();
    create_logical_device(config::instance::surface);
    create_sync_objects();
    create_descriptors();
    create_swap_chain(window);
    create_image_views();
    create_command_pool();
}

auto gse::vulkan::get_next_image(GLFWwindow* window) -> std::uint32_t {
    assert(config::device::device.waitForFences(1, &config::sync::in_flight_fence, true, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess, "Failed to wait for fence!");
    assert(config::device::device.resetFences(1, &config::sync::in_flight_fence) != vk::Result::eSuccess, "Failed to reset fence!");

    std::uint32_t image_index = 0;
    const auto result = config::device::device.acquireNextImageKHR(config::swap_chain::swap_chain, std::numeric_limits<std::uint64_t>::max(), config::sync::image_available_semaphore, {}, &image_index);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        create_swap_chain(window);
        return get_next_image(window);
    }

    config::sync::current_frame = (config::sync::current_frame + 1) % max_frames_in_flight;

    return image_index;
}

auto gse::vulkan::find_memory_type(const std::uint32_t type_filter, const vk::MemoryPropertyFlags properties) -> std::uint32_t {
    /*const auto memory_properties = config::device::physical_device.getMemoryProperties();
    for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if (type_filter & 1 << i && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");*/
	return 0;
}

auto gse::vulkan::begin_single_line_commands() -> vk::CommandBuffer {
    const vk::CommandBufferAllocateInfo alloc_info(
	    config::command::pool, vk::CommandBufferLevel::ePrimary, 1
    );

    const vk::CommandBuffer command_buffer = config::device::device.allocateCommandBuffers(alloc_info)[0];
    constexpr vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    );

    command_buffer.begin(begin_info);
    return command_buffer;
}

auto gse::vulkan::end_single_line_commands(const vk::CommandBuffer command_buffer) -> void {
    command_buffer.end();
    const vk::SubmitInfo submit_info(
        0, nullptr, nullptr, 1, &command_buffer, 0, nullptr
    );

    config::queue::graphics.submit(submit_info, nullptr);
    config::queue::graphics.waitIdle();
    config::device::device.freeCommandBuffers(config::command::pool, command_buffer);
}

auto gse::vulkan::create_buffer(const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::DeviceMemory& buffer_memory) -> vk::Buffer {
    const vk::BufferCreateInfo buffer_info(
        {}, size, usage, vk::SharingMode::eExclusive
    );

    const vk::Buffer buffer = config::device::device.createBuffer(buffer_info);
    const vk::MemoryRequirements mem_requirements = config::device::device.getBufferMemoryRequirements(buffer);
    const vk::MemoryAllocateInfo alloc_info(
        mem_requirements.size, find_memory_type(mem_requirements.memoryTypeBits, properties)
    );

    buffer_memory = config::device::device.allocateMemory(alloc_info);
    config::device::device.bindBufferMemory(buffer, buffer_memory, 0);

    return buffer;
}

auto gse::vulkan::shutdown() -> void {
    config::device::device.waitIdle();
    config::device::device.destroyDescriptorPool(config::descriptor::pool);

    config::device::device.destroySemaphore(config::sync::image_available_semaphore);
    config::device::device.destroySemaphore(config::sync::render_finished_semaphore);
    config::device::device.destroyFence(config::sync::in_flight_fence);

    for (const auto& image_view : config::swap_chain::image_views) {
        config::device::device.destroyImageView(image_view);
    }

    config::device::device.destroySwapchainKHR(config::swap_chain::swap_chain);
    config::device::device.destroy();

    config::instance::instance.destroySurfaceKHR(config::instance::surface);
    config::instance::instance.destroy();
}

auto gse::vulkan::select_gpu() -> void {
    const auto devices = config::instance::instance.enumeratePhysicalDevices();

    assert(!devices.empty(), "No Vulkan-compatible GPUs found!");
    std::cout << "Found " << devices.size() << " Vulkan-compatible GPU(s):\n";

    for (const auto& device : devices) {
        vk::PhysicalDeviceProperties properties = device.getProperties();
        std::cout << " - " << properties.deviceName << " (Type: " << to_string(properties.deviceType) << ")\n";
    }

    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            config::device::physical_device = device;
            break;
        }
    }

    if (!config::device::physical_device) {
        config::device::physical_device = devices[0];
    }

    std::cout << "Selected GPU: " << config::device::physical_device.getProperties().deviceName << "\n";
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
    auto [graphics_family, present_family] = find_queue_families(config::device::physical_device, surface);

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

    vk::PhysicalDeviceFeatures2 features2{};
    vk::PhysicalDeviceVulkan12Features vulkan12_features{};
    vk::PhysicalDeviceVulkan13Features vulkan13_features{};

    features2.pNext = &vulkan12_features;
    vulkan12_features.pNext = &vulkan13_features;

    config::device::physical_device.getFeatures2(&features2);

    features2.features.samplerAnisotropy = vk::True;
    vulkan12_features.timelineSemaphore = vk::True;
    vulkan12_features.bufferDeviceAddress = vk::True;
    vulkan13_features.synchronization2 = vk::True;

    const std::vector device_extensions = {
        vk::KHRSwapchainExtensionName
    };

    vk::DeviceCreateInfo create_info(
        {},
        static_cast<uint32_t>(queue_create_infos.size()),
        queue_create_infos.data(),
        0, nullptr,
        static_cast<uint32_t>(device_extensions.size()),
        device_extensions.data()
    );
    create_info.pNext = &features2;

    config::device::device = config::device::physical_device.createDevice(create_info);
    config::queue::graphics = config::device::device.getQueue(graphics_family.value(), 0);
    config::queue::present = config::device::device.getQueue(present_family.value(), 0);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#endif

    std::cout << "Logical Device Created Successfully!\n";
}

auto gse::vulkan::query_swap_chain_support(const vk::PhysicalDevice device, const vk::SurfaceKHR surface) -> config::swap_chain_details {
    return {
        device.getSurfaceCapabilitiesKHR(surface),
        device.getSurfaceFormatsKHR(surface),
        device.getSurfacePresentModesKHR(surface)
    };
}

auto gse::vulkan::choose_swap_chain_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) -> vk::SurfaceFormatKHR {
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available_format;
        }
    }
    return available_formats[0];
}

auto gse::vulkan::choose_swap_chain_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) -> vk::PresentModeKHR {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == vk::PresentModeKHR::eMailbox) {
            return available_present_mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

auto gse::vulkan::create_swap_chain(GLFWwindow* window) -> void {
    config::swap_chain::details = query_swap_chain_support(config::device::physical_device, config::instance::surface);

    config::swap_chain::surface_format = choose_swap_chain_surface_format(config::swap_chain::details.formats);
    config::swap_chain::present_mode = choose_swap_chain_present_mode(config::swap_chain::details.present_modes);
    config::swap_chain::extent = choose_swap_chain_extent(config::swap_chain::details.capabilities, window);

    std::uint32_t image_count = config::swap_chain::details.capabilities.minImageCount + 1;
    if (config::swap_chain::details.capabilities.maxImageCount > 0 && image_count > config::swap_chain::details.capabilities.maxImageCount) {
        image_count = config::swap_chain::details.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info(
        {},
        config::instance::surface,
        image_count,
        config::swap_chain::surface_format.format,
        config::swap_chain::surface_format.colorSpace,
        config::swap_chain::extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment
    );

    auto [graphics_family, present_family] = find_queue_families(config::device::physical_device, config::instance::surface);
    const std::uint32_t queue_family_indices[] = { graphics_family.value(), present_family.value() };

    if (graphics_family != present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = config::swap_chain::details.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = config::swap_chain::present_mode;
    create_info.clipped = true;

    config::swap_chain::swap_chain = config::device::device.createSwapchainKHR(create_info);

    config::swap_chain::images = config::device::device.getSwapchainImagesKHR(config::swap_chain::swap_chain);
    config::swap_chain::format = config::swap_chain::surface_format.format;

    config::swap_chain::frame_buffers.resize(config::swap_chain::images.size());

    for (size_t i = 0; i < config::swap_chain::image_views.size(); i++) {
        vk::ImageView attachments[] = {
	        config::swap_chain::image_views[i]
        };

        vk::FramebufferCreateInfo framebuffer_info(
            {},
	        config::render_pass,
            static_cast<uint32_t>(std::size(attachments)),
            attachments,
	        config::swap_chain::extent.width,
	        config::swap_chain::extent.height,
            1
        );

        config::swap_chain::frame_buffers[i] = config::device::device.createFramebuffer(framebuffer_info);
    }

    std::cout << "Swapchain Created Successfully!\n";
}

auto gse::vulkan::create_image_views() -> void {
    config::swap_chain::image_views.resize(config::swap_chain::images.size());

    for (size_t i = 0; i < config::swap_chain::images.size(); i++) {
        vk::ImageViewCreateInfo image_create_info(
            {},
            config::swap_chain::images[i],
            vk::ImageViewType::e2D,
            config::swap_chain::format,
            {
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity
            },
            {
                vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
            }
        );

        config::swap_chain::image_views[i] = config::device::device.createImageView(image_create_info);
    }

    std::cout << "Image Views Created Successfully!\n";
}

auto gse::vulkan::create_sync_objects() -> void {
    constexpr vk::SemaphoreCreateInfo semaphore_info;
    constexpr vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);

    config::sync::image_available_semaphore = config::device::device.createSemaphore(semaphore_info);
    config::sync::render_finished_semaphore = config::device::device.createSemaphore(semaphore_info);
    config::sync::in_flight_fence = config::device::device.createFence(fence_info);

    std::cout << "Sync Objects Created Successfully!\n";
}

auto gse::vulkan::create_descriptors() -> void {
    constexpr uint32_t max_sets = 512;

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eUniformBuffer,         1024 },  // enough for global UBOs + per-object data
        { vk::DescriptorType::eCombinedImageSampler,  1024 },  // textures for materials, post fx, etc.
        { vk::DescriptorType::eStorageBuffer,          256 },  // e.g., light buffer, SSBOs
    };

    const vk::DescriptorPoolCreateInfo pool_info{
        {},
        max_sets,
        static_cast<uint32_t>(pool_sizes.size()),
        pool_sizes.data()
    };

    config::descriptor::pool = config::device::device.createDescriptorPool(pool_info);

    std::cout << "Descriptor Pool Created Successfully!\n";
}

auto gse::vulkan::create_command_pool() -> void {
    const auto [graphics_family, present_family] = find_queue_families(config::device::physical_device, config::instance::surface);

    const vk::CommandPoolCreateInfo pool_info(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        graphics_family.value()
    );

    config::command::pool = config::device::device.createCommandPool(pool_info);

    std::cout << "Command Pool Created Successfully!\n";
}