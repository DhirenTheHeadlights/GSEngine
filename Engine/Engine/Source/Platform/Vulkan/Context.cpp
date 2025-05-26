module;

#include <compare>

module gse.platform.vulkan.context;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.vulkan.uploader;
import gse.platform.assert;

auto gse::vulkan::initialize(GLFWwindow* window) -> config {
    auto configuration = create_instance(window);

    select_gpu(configuration);
    create_logical_device(configuration);
    create_sync_objects(configuration);
    create_descriptors(configuration);
    create_swap_chain(window, configuration);
    create_image_views(configuration);
    create_command_pool(configuration);

    return configuration;
}

auto gse::vulkan::begin_frame(GLFWwindow* window, config& config) -> void {
    const auto& device = config.device_data.device;

    assert(device.waitForFences(1, &config.sync.in_flight_fence, true, std::numeric_limits<std::uint64_t>::max()) == vk::Result::eSuccess, "Failed to wait for fence!");
    assert(device.resetFences(1, &config.sync.in_flight_fence) == vk::Result::eSuccess, "Failed to reset fence!");

    std::uint32_t image_index;
    const auto result = device.acquireNextImageKHR(
        config.swap_chain_data.swap_chain,
        std::numeric_limits<std::uint64_t>::max(),
        config.sync.image_available_semaphore,
        {},
        &image_index
    );

    assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Failed to acquire swap chain image!");

    if (result == vk::Result::eErrorOutOfDateKHR) {
        create_swap_chain(window, config);
		create_image_views(config);
        return begin_frame(window, config);
    }

    config.frame_context = {
        .image_index = image_index,
        .command_buffer = config.command.buffers[image_index],
        .framebuffer = config.swap_chain_data.frame_buffers[image_index]
	};

    constexpr vk::CommandBufferBeginInfo begin_info{};
    config.frame_context.command_buffer.begin(begin_info);

    vk::ClearValue clear{ std::array{0.1f, 0.1f, 0.1f, 1.0f} };
    const vk::RenderPassBeginInfo render_pass_begin_info{
        config.render_pass,
        config.frame_context.framebuffer,
        { { 0,0 }, config.swap_chain_data.extent },
        1, &clear
    };

    config.frame_context.command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
}

auto gse::vulkan::end_frame(const config& config) -> void {
    config.frame_context.command_buffer.endRenderPass();
    config.frame_context.command_buffer.end();

    const vk::Semaphore wait_semaphores[] = { config.sync.image_available_semaphore };
    constexpr vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    const vk::SubmitInfo submit_info(
        1, wait_semaphores, wait_stages,
        1, &config.frame_context.command_buffer,
        1, &config.sync.render_finished_semaphore
    );

    config.queue.graphics.submit(submit_info, config.sync.in_flight_fence);

    const vk::PresentInfoKHR present_info(
        1, &config.sync.render_finished_semaphore,
        1, &config.swap_chain_data.swap_chain,
        &config.frame_context.image_index
    );

    const vk::Result present_result = config.queue.present.presentKHR(present_info);

    assert(
        present_result == vk::Result::eSuccess || present_result == vk::Result::eSuboptimalKHR,
        "Failed to present image!"
    );
}

auto gse::vulkan::begin_single_line_commands(const config& config) -> vk::CommandBuffer {
    const vk::CommandBufferAllocateInfo alloc_info(
	    config.command.pool, vk::CommandBufferLevel::ePrimary, 1
    );

    const vk::CommandBuffer command_buffer = config.device_data.device.allocateCommandBuffers(alloc_info)[0];
    constexpr vk::CommandBufferBeginInfo begin_info;

    command_buffer.begin(begin_info);
    return command_buffer;
}

auto gse::vulkan::end_single_line_commands(const vk::CommandBuffer command_buffer, const config& config) -> void {
    command_buffer.end();
    const vk::SubmitInfo submit_info(
        0, nullptr, nullptr, 1, &command_buffer, 0, nullptr
    );

    config.queue.graphics.submit(submit_info, nullptr);
    config.queue.graphics.waitIdle();
    config.device_data.device.freeCommandBuffers(config.command.pool, command_buffer);
}

auto gse::vulkan::shutdown(const config& config) -> void {
    const auto& device = config.device_data.device;

    device.waitIdle();
    device.destroyRenderPass(config.render_pass);

    for (const auto& framebuffer : config.swap_chain_data.frame_buffers) {
        device.destroyFramebuffer(framebuffer);
    }

    for (const auto& image_view : config.swap_chain_data.image_views) {
        device.destroyImageView(image_view);
    }

    device.destroyDescriptorPool(config.descriptor.pool);
    device.destroyCommandPool(config.command.pool);

    device.destroySemaphore(config.sync.image_available_semaphore);
    device.destroySemaphore(config.sync.render_finished_semaphore);
    device.destroyFence(config.sync.in_flight_fence);

    device.destroySwapchainKHR(config.swap_chain_data.swap_chain);
    device.destroy();

    config.instance_data.instance.destroySurfaceKHR(config.instance_data.surface);
    config.instance_data.instance.destroy();
}

auto gse::vulkan::select_gpu(config& config) -> void {
    const auto devices = config.instance_data.instance.enumeratePhysicalDevices();

    assert(!devices.empty(), "No Vulkan-compatible GPUs found!");
    std::cout << "Found " << devices.size() << " Vulkan-compatible GPU(s):\n";

    for (const auto& device : devices) {
        vk::PhysicalDeviceProperties properties = device.getProperties();
        std::cout << " - " << properties.deviceName << " (Type: " << to_string(properties.deviceType) << ")\n";
    }

    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            config.device_data.physical_device = device;
            break;
        }
    }

    if (!config.device_data.physical_device) {
        config.device_data.physical_device = devices[0];
    }

    std::cout << "Selected GPU: " << config.device_data.physical_device.getProperties().deviceName << "\n";
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

auto gse::vulkan::create_logical_device(config& config) -> void {
    auto [graphics_family, present_family] = find_queue_families(config.device_data.physical_device, config.instance_data.surface);

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

    config.device_data.physical_device.getFeatures2(&features2);

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

    config.device_data.device = config.device_data.physical_device.createDevice(create_info);
    config.queue.graphics = config.device_data.device.getQueue(graphics_family.value(), 0);
    config.queue.present = config.device_data.device.getQueue(present_family.value(), 0);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
    VULKAN_HPP_DEFAULT_DISPATCHER.init(config.device_data.device);
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

auto gse::vulkan::create_swap_chain(GLFWwindow* window, config& config) -> void {
    config.swap_chain_data.details = query_swap_chain_support(config.device_data.physical_device, config.instance_data.surface);

    config.swap_chain_data.surface_format = choose_swap_chain_surface_format(config.swap_chain_data.details.formats);
    config.swap_chain_data.present_mode = choose_swap_chain_present_mode(config.swap_chain_data.details.present_modes);
    config.swap_chain_data.extent = choose_swap_chain_extent(config.swap_chain_data.details.capabilities, window);

    std::uint32_t image_count = config.swap_chain_data.details.capabilities.minImageCount + 1;
    if (config.swap_chain_data.details.capabilities.maxImageCount > 0 && image_count > config.swap_chain_data.details.capabilities.maxImageCount) {
        image_count = config.swap_chain_data.details.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info(
        {},
        config.instance_data.surface,
        image_count,
        config.swap_chain_data.surface_format.format,
        config.swap_chain_data.surface_format.colorSpace,
        config.swap_chain_data.extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment
    );

    auto [graphics_family, present_family] = find_queue_families(config.device_data.physical_device, config.instance_data.surface);
    const std::uint32_t queue_family_indices[] = { graphics_family.value(), present_family.value() };

    if (graphics_family != present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = config.swap_chain_data.details.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = config.swap_chain_data.present_mode;
    create_info.clipped = true;

    config.swap_chain_data.swap_chain = config.device_data.device.createSwapchainKHR(create_info);
    config.swap_chain_data.images = config.device_data.device.getSwapchainImagesKHR(config.swap_chain_data.swap_chain);
    config.swap_chain_data.format = config.swap_chain_data.surface_format.format;
    config.swap_chain_data.frame_buffers.resize(config.swap_chain_data.images.size());
}

auto gse::vulkan::create_image_views(config& config) -> void {
    config.swap_chain_data.image_views.resize(config.swap_chain_data.images.size());

    for (size_t i = 0; i < config.swap_chain_data.images.size(); i++) {
        vk::ImageViewCreateInfo image_create_info(
            {},
            config.swap_chain_data.images[i],
            vk::ImageViewType::e2D,
            config.swap_chain_data.format,
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

        config.swap_chain_data.image_views[i] = config.device_data.device.createImageView(image_create_info);
    }

    std::cout << "Image Views Created Successfully!\n";

    vk::AttachmentDescription color_attachment{
        {},
        config.swap_chain_data.format,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference color_ref{ 0, vk::ImageLayout::eColorAttachmentOptimal };

    vk::SubpassDescription sub_pass{
        {}, vk::PipelineBindPoint::eGraphics,
        0, nullptr,
        1, &color_ref,
        nullptr, nullptr
    };

    vk::SubpassDependency dep{
        vk::SubpassExternal,
        0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite,
    };

    const vk::RenderPassCreateInfo rp_info{
        {}, 1, &color_attachment,
		1, &sub_pass, 1, &dep
    };

    config.render_pass = config.device_data.device.createRenderPass(rp_info);

    std::cout << "Render Pass Created Successfully!\n";

    for (size_t i = 0; i < config.swap_chain_data.image_views.size(); i++) {
        vk::ImageView attachments[] = {
            config.swap_chain_data.image_views[i]
        };

        vk::FramebufferCreateInfo framebuffer_info(
            {},
            config.render_pass,
            static_cast<uint32_t>(std::size(attachments)),
            attachments,
            config.swap_chain_data.extent.width,
            config.swap_chain_data.extent.height,
            1
        );

        config.swap_chain_data.frame_buffers[i] = config.device_data.device.createFramebuffer(framebuffer_info);
    }

    std::cout << "Swapchain Created Successfully!\n";
}

auto gse::vulkan::create_sync_objects(config& config) -> void {
    constexpr vk::SemaphoreCreateInfo semaphore_info;
    constexpr vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);

    config.sync.image_available_semaphore = config.device_data.device.createSemaphore(semaphore_info);
    config.sync.render_finished_semaphore = config.device_data.device.createSemaphore(semaphore_info);
    config.sync.in_flight_fence = config.device_data.device.createFence(fence_info);

    std::cout << "Sync Objects Created Successfully!\n";
}

auto gse::vulkan::create_descriptors(config& config) -> void {
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

    config.descriptor.pool = config.device_data.device.createDescriptorPool(pool_info);

    std::cout << "Descriptor Pool Created Successfully!\n";
}

auto gse::vulkan::create_command_pool(config& config) -> void {
    const auto [graphics_family, present_family] = find_queue_families(config.device_data.physical_device, config.instance_data.surface);

    const vk::CommandPoolCreateInfo pool_info(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        graphics_family.value()
    );

    config.command.pool = config.device_data.device.createCommandPool(pool_info);

    std::cout << "Command Pool Created Successfully!\n";

    config.command.buffers.resize(config.swap_chain_data.frame_buffers.size());

    const vk::CommandBufferAllocateInfo alloc_info(
        config.command.pool, vk::CommandBufferLevel::ePrimary,
        static_cast<std::uint32_t>(config.command.buffers.size())
    );

    config.command.buffers = config.device_data.device.allocateCommandBuffers(alloc_info);

    std::cout << "Command Buffers Created Successfully!\n";
}