module;

#include <algorithm>
#include <vulkan/vulkan_hpp_macros.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <vector>

module gse.platform.vulkan.context;

import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.vulkan.uploader;
import gse.platform.vulkan.persistent_allocator;
import gse.platform.assert;

std::uint32_t g_max_frames_in_flight = 2;

auto gse::vulkan::initialize(GLFWwindow* window) -> config {
    auto instance_data = create_instance_and_surface(window);
    auto [device_data, queue] = create_device_and_queues(instance_data);
    auto descriptor = create_descriptor_pool(device_data);
    auto swap_chain_data = create_swap_chain_resources(window, instance_data, device_data);
    auto command = create_command_objects(device_data, instance_data, swap_chain_data.images.size());
    auto sync = create_sync_objects(device_data, swap_chain_data.images.size());

    config::frame_context_config frame_context(
        0,
        *command.buffers[0],
        *swap_chain_data.frame_buffers[0]
    );

    return config(
        std::move(instance_data),
        std::move(device_data),
        std::move(queue),
        std::move(command),
        std::move(descriptor),
        std::move(sync),
        std::move(swap_chain_data),
        std::move(frame_context)
    );
}

auto gse::vulkan::begin_frame(GLFWwindow* window, config& config) -> void {
    const auto& device = config.device_data.device;

    assert(
        device.waitForFences(
            *config.sync.in_flight_fences[config.current_frame],
            vk::True,
            std::numeric_limits<std::uint64_t>::max()
        ) == vk::Result::eSuccess,
        "Failed to wait for in-flight fence!"
    );

    vk::Result result;
    std::uint32_t image_index;

    try {
        const vk::AcquireNextImageInfoKHR acquire_info(
            config.swap_chain_data.swap_chain,
            std::numeric_limits<std::uint64_t>::max(),
            config.sync.image_available_semaphores[config.current_frame],
            nullptr,
            1
        );
        std::tie(result, image_index) = device.acquireNextImage2KHR(acquire_info);
    }
    catch (const vk::OutOfDateKHRError& e) {
        result = vk::Result::eErrorOutOfDateKHR;
    }

    if (result == vk::Result::eErrorOutOfDateKHR) {
        config.device_data.device.waitIdle();
        config.swap_chain_data = create_swap_chain_resources(window, config.instance_data, config.device_data);
		config.sync = create_sync_objects(config.device_data, config.swap_chain_data.images.size());
        return;
    }

    assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Failed to acquire swap chain image!");

    if (config.sync.images_in_flight[image_index] != nullptr) {
        assert(
            device.waitForFences(
                config.sync.images_in_flight[image_index],
                vk::True,
                std::numeric_limits<std::uint64_t>::max()
            ) == vk::Result::eSuccess,
            "Failed to wait for in-flight image fence!"
        );
    }

    config.sync.images_in_flight[image_index] = config.sync.in_flight_fences[config.current_frame];

    device.resetFences(*config.sync.in_flight_fences[config.current_frame]);

    config.frame_context = {
        image_index,
        config.command.buffers[config.current_frame],
        config.swap_chain_data.frame_buffers[image_index]
    };

    config.frame_context.command_buffer.reset({});

    constexpr vk::CommandBufferBeginInfo begin_info{};
    config.frame_context.command_buffer.begin(begin_info);

    std::array<vk::ClearValue, 5> clear_values;
    clear_values[0].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
    clear_values[1].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
    clear_values[2].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
    clear_values[3].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
    clear_values[4].color = vk::ClearColorValue(std::array{ 0.1f, 0.1f, 0.1f, 1.0f });

    const vk::RenderPassBeginInfo render_pass_begin_info(
        config.swap_chain_data.render_pass,
        config.frame_context.framebuffer,
        { {0, 0}, config.swap_chain_data.extent },
        static_cast<std::uint32_t>(clear_values.size()),
        clear_values.data()
    );

    config.frame_context.command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
}

auto gse::vulkan::end_frame(GLFWwindow* window, config& config) -> void {
    config.frame_context.command_buffer.endRenderPass();
    config.frame_context.command_buffer.end();

    const std::uint32_t image_index = config.frame_context.image_index;

    const vk::SemaphoreSubmitInfo wait_info(
        *config.sync.image_available_semaphores[config.current_frame], 
        0,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        0
    );

    const vk::CommandBufferSubmitInfo cmd_info(
        config.frame_context.command_buffer
    );

    const vk::SemaphoreSubmitInfo signal_info(
        *config.sync.render_finished_semaphores[image_index],         
        0,                                                            
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        0
    );                                                            

    const vk::SubmitInfo2 submit_info2(
        {},                 
        1, &wait_info,         
        1, &cmd_info,            
        1, &signal_info
    );        

    config.queue.graphics.submit2(
        submit_info2,
        config.sync.in_flight_fences[config.current_frame]
    );

    const vk::PresentInfoKHR present_info(
        1,
        &*config.sync.render_finished_semaphores[image_index],
        1,
        &*config.swap_chain_data.swap_chain,
        &image_index
    );

    const vk::Result present_result = config.queue.present.presentKHR(present_info);

    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        config.device_data.device.waitIdle();
        config.swap_chain_data =
            create_swap_chain_resources(window,
                config.instance_data,
                config.device_data);
        config.sync =
            create_sync_objects(config.device_data,
                config.swap_chain_data.images.size());
        return;
    }

    assert(
        present_result == vk::Result::eSuccess,
        "Failed to present swap chain image!"
    );

    config.current_frame = (config.current_frame + 1) % g_max_frames_in_flight;
}

auto gse::vulkan::begin_single_line_commands(const config& config) -> vk::raii::CommandBuffer {
    const vk::CommandBufferAllocateInfo alloc_info(
	    config.command.pool, vk::CommandBufferLevel::ePrimary, 1
    );

    auto buffers = config.device_data.device.allocateCommandBuffers(alloc_info);
	vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

    command_buffer.begin({});
    return command_buffer;
}

auto gse::vulkan::end_single_line_commands(const vk::raii::CommandBuffer& command_buffer, const config& config) -> void {
    command_buffer.end();
    const vk::SubmitInfo submit_info(
        0, nullptr, nullptr, 1, &*command_buffer, 0, nullptr
    );

    config.queue.graphics.submit(submit_info, nullptr);
    config.queue.graphics.waitIdle();
}

auto gse::vulkan::shutdown(const config& config) -> void {
    config.device_data.device.waitIdle();
}

auto debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity, vk::DebugUtilsMessageTypeFlagsEXT message_type, const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) -> vk::Bool32 {
    std::cerr << "Validation layer: " << callback_data->pMessage << "\n";
    return vk::False;
}

auto gse::vulkan::create_instance_and_surface(GLFWwindow* window) -> config::instance_config {
    const std::vector validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

    const uint32_t highest_supported_version = vk::enumerateInstanceVersion();
    const vk::ApplicationInfo app_info(
        "GSEngine", 1,
        "GSEngine", 1,
        highest_supported_version
    );

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info(
        {},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debug_callback)
    );

    constexpr vk::ValidationFeatureEnableEXT enables[] = {
        vk::ValidationFeatureEnableEXT::eBestPractices,
        vk::ValidationFeatureEnableEXT::eGpuAssisted,
        vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation
    };

    const vk::ValidationFeaturesEXT features(
        std::size(enables),
        enables,
        0, nullptr,
        &debug_create_info
    );

    const vk::InstanceCreateInfo create_info(
        {},
        &app_info,
        static_cast<uint32_t>(validation_layers.size()),
        validation_layers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data(),
        &features
    );

    vk::raii::Instance instance = nullptr;
    vk::raii::Context context;

    try {
	    instance = vk::raii::Instance(context, create_info);
        std::cout << "Vulkan Instance Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Vulkan instance: " << err.what() << "\n";
        throw;
    }

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
#endif

    try {
        g_debug_utils_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
        std::cout << "Debug Messenger Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Debug Messenger: " << err.what() << "\n";
    }

    VkSurfaceKHR temp_surface;
    assert(glfwCreateWindowSurface(*instance, window, nullptr, &temp_surface) == VK_SUCCESS, "Error creating window surface");

    vk::raii::SurfaceKHR surface(instance, temp_surface);

    return config::instance_config(std::move(context), std::move(instance), std::move(surface));
}

auto gse::vulkan::create_device_and_queues(const config::instance_config& instance_data) -> std::tuple<config::device_config, config::queue_config> {
    const auto devices = instance_data.instance.enumeratePhysicalDevices();
    assert(!devices.empty(), "No Vulkan-compatible GPUs found!");

    vk::raii::PhysicalDevice physical_device = nullptr;

    for (const auto& device : devices) {
        if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            physical_device = device;
            break;
        }
    }

    if (!*physical_device) {
        physical_device = devices[0];
    }

    std::cout << "Selected GPU: " << physical_device.getProperties().deviceName << "\n";

    auto [graphics_family, present_family] = find_queue_families(physical_device, instance_data.surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set unique_queue_families = {
        graphics_family.value(),
        present_family.value()
    };

    float queue_priority = 1.0f;
    for (uint32_t queue_family_index : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info({}, queue_family_index, 1, &queue_priority);
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceVulkan13Features vulkan13_features{};
    vk::PhysicalDeviceVulkan12Features vulkan12_features{};
    vulkan12_features.pNext = &vulkan13_features;

	vk::PhysicalDeviceFeatures2 features2 = physical_device.getFeatures2();
    features2.pNext = &vulkan12_features;

    features2.features.samplerAnisotropy = vk::True;
    vulkan12_features.timelineSemaphore = vk::True;
    vulkan12_features.bufferDeviceAddress = vk::True;
    vulkan13_features.synchronization2 = vk::True;

    const std::vector device_extensions = {
        vk::KHRSwapchainExtensionName,
		vk::KHRPushDescriptorExtensionName,
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

    vk::raii::Device device = physical_device.createDevice(create_info);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
#endif

    std::cout << "Logical Device Created Successfully!\n";

    vk::raii::Queue graphics_queue = device.getQueue(graphics_family.value(), 0);
    vk::raii::Queue present_queue = device.getQueue(present_family.value(), 0);

    auto device_conf = config::device_config(std::move(physical_device), std::move(device));
    auto queue_conf = config::queue_config(std::move(graphics_queue), std::move(present_queue));

    return std::make_tuple(std::move(device_conf), std::move(queue_conf));
}

auto gse::vulkan::create_command_objects(const config::device_config& device_data, const config::instance_config& instance_data, std::uint32_t image_count) -> config::command_config {
    const auto [graphics_family, present_family] = find_queue_families(device_data.physical_device, instance_data.surface);

    const vk::CommandPoolCreateInfo pool_info(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        graphics_family.value()
    );

    vk::raii::CommandPool pool = device_data.device.createCommandPool(pool_info);

    std::cout << "Command Pool Created Successfully!\n";

    const vk::CommandBufferAllocateInfo alloc_info(
        *pool,
        vk::CommandBufferLevel::ePrimary,
        image_count
    );

    std::vector<vk::raii::CommandBuffer> buffers = device_data.device.allocateCommandBuffers(alloc_info);

    std::cout << "Command Buffers Created Successfully!\n";

    return config::command_config(std::move(pool), std::move(buffers));
}

auto gse::vulkan::create_descriptor_pool(const config::device_config& device_data) -> config::descriptor_config {
    constexpr uint32_t max_sets = 512;

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eUniformBuffer,         4096 },
        { vk::DescriptorType::eCombinedImageSampler,  4096 },
        { vk::DescriptorType::eStorageBuffer,         1024 },
        { vk::DescriptorType::eInputAttachment,       4096 },
    };

    const vk::DescriptorPoolCreateInfo pool_info{
        {},
        max_sets,
        static_cast<uint32_t>(pool_sizes.size()),
        pool_sizes.data()
    };

    vk::raii::DescriptorPool pool = device_data.device.createDescriptorPool(pool_info);

    std::cout << "Descriptor Pool Created Successfully!\n";

    return config::descriptor_config(std::move(pool));
}

auto gse::vulkan::create_sync_objects(const config::device_config& device_data, const std::uint32_t image_count) -> config::sync_config {
    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    image_available_semaphores.reserve(g_max_frames_in_flight);
    render_finished_semaphores.reserve(image_count);
    in_flight_fences.reserve(g_max_frames_in_flight);

    constexpr vk::SemaphoreCreateInfo semaphore_info{};
    constexpr vk::FenceCreateInfo fence_info{ vk::FenceCreateFlagBits::eSignaled };

    for (size_t i = 0; i < g_max_frames_in_flight; ++i) {
        image_available_semaphores.emplace_back(device_data.device, semaphore_info);
        in_flight_fences.emplace_back(device_data.device, fence_info);
    }

    for (size_t i = 0; i < image_count; ++i) {
        render_finished_semaphores.emplace_back(device_data.device, semaphore_info);
	}

    std::vector<vk::Fence> images_in_flight(image_count, nullptr);

    std::cout << "Sync Objects Created Successfully!\n";

    return config::sync_config(
        std::move(image_available_semaphores),
        std::move(render_finished_semaphores),
        std::move(in_flight_fences),
        std::move(images_in_flight)
    );
}

auto gse::vulkan::find_queue_families(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> queue_family {
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

auto gse::vulkan::create_swap_chain_resources(GLFWwindow* window, const config::instance_config& instance_data, const config::device_config& device_data) -> config::swap_chain_config {
    config::swap_chain_details details = {
        device_data.physical_device.getSurfaceCapabilitiesKHR(*instance_data.surface),
        device_data.physical_device.getSurfaceFormatsKHR(*instance_data.surface),
        device_data.physical_device.getSurfacePresentModesKHR(*instance_data.surface)
    };

    vk::SurfaceFormatKHR surface_format;
    for (const auto& available_format : details.formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surface_format = available_format;
            break;
        }
        surface_format = details.formats[0];
    }

    auto present_mode = vk::PresentModeKHR::eFifo; 
    for (const auto& available_present_mode : details.present_modes) {
        if (available_present_mode == vk::PresentModeKHR::eMailbox) {
            present_mode = available_present_mode;
            break;
        }
    }

    vk::Extent2D extent;

    if (details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = details.capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        extent.width = std::clamp(actual_extent.width, details.capabilities.minImageExtent.width, details.capabilities.maxImageExtent.width);
        extent.height = std::clamp(actual_extent.height, details.capabilities.minImageExtent.height, details.capabilities.maxImageExtent.height);
    }

    std::uint32_t image_count = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount) {
        image_count = details.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info(
        {}, *instance_data.surface, image_count, surface_format.format, surface_format.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment
    );

    auto [graphics_family, present_family] = find_queue_families(device_data.physical_device, instance_data.surface);
    const std::uint32_t queue_family_indices[] = { graphics_family.value(), present_family.value() };

    if (graphics_family != present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = details.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = true;

    auto swap_chain = device_data.device.createSwapchainKHR(create_info);
    auto images = swap_chain.getImages();
    auto format = surface_format.format;

    vk::ImageViewCreateInfo position_iv_info(
        {}, nullptr, vk::ImageViewType::e2D, vk::Format::eR16G16B16A16Sfloat, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );
    auto position_image = persistent_allocator::create_image(
        device_data, { {}, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, { extent.width, extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment }, vk::MemoryPropertyFlagBits::eDeviceLocal, position_iv_info
    );

    vk::ImageViewCreateInfo normal_iv_info(
        {}, nullptr, vk::ImageViewType::e2D, vk::Format::eR16G16B16A16Sfloat, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );
    auto normal_image = persistent_allocator::create_image(
        device_data, { {}, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, { extent.width, extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment }, vk::MemoryPropertyFlagBits::eDeviceLocal, normal_iv_info
    );

    vk::ImageViewCreateInfo albedo_iv_info(
        {}, nullptr, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );
    auto albedo_image = persistent_allocator::create_image(
        device_data, { {}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { extent.width, extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment }, vk::MemoryPropertyFlagBits::eDeviceLocal, albedo_iv_info
    );

    vk::ImageViewCreateInfo depth_iv_info(
        {}, nullptr, vk::ImageViewType::e2D, vk::Format::eD32Sfloat, {},
        { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
    );
    auto depth_image = persistent_allocator::create_image(
        device_data, { {}, vk::ImageType::e2D, vk::Format::eD32Sfloat, { extent.width, extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment }, vk::MemoryPropertyFlagBits::eDeviceLocal, depth_iv_info
    );

    std::vector<vk::raii::ImageView> image_views;
    image_views.reserve(images.size());
    for (const auto& image : images) {
        vk::ImageViewCreateInfo iv_create_info(
            {}, image, vk::ImageViewType::e2D, format, {},
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        );
        image_views.emplace_back(device_data.device, iv_create_info);
    }

    vk::AttachmentDescription attachments[5] = {
        vk::AttachmentDescription(
            {}, vk::Format::eR16G16B16A16Sfloat, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
        ),
        vk::AttachmentDescription(
            {}, vk::Format::eR16G16B16A16Sfloat, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
        ),
        vk::AttachmentDescription(
            {}, vk::Format::eR8G8B8A8Srgb, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
        ),
        vk::AttachmentDescription(
            {}, vk::Format::eD32Sfloat, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal
        ),
        vk::AttachmentDescription(
            {}, format, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR
        )
    };

    vk::AttachmentReference gbuffer_color_refs[] = {
        { 0, vk::ImageLayout::eColorAttachmentOptimal },
        { 1, vk::ImageLayout::eColorAttachmentOptimal },
        { 2, vk::ImageLayout::eColorAttachmentOptimal }
    };
    constexpr vk::AttachmentReference depth_ref{ 3, vk::ImageLayout::eDepthStencilAttachmentOptimal };
    const vk::SubpassDescription gbuffer_pass({}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 3, gbuffer_color_refs, nullptr, &depth_ref);

    vk::AttachmentReference lighting_input_refs[] = {
        { 0, vk::ImageLayout::eShaderReadOnlyOptimal },
        { 1, vk::ImageLayout::eShaderReadOnlyOptimal },
        { 2, vk::ImageLayout::eShaderReadOnlyOptimal }
    };
    vk::AttachmentReference lighting_color_ref{ 4, vk::ImageLayout::eColorAttachmentOptimal };
    const vk::SubpassDescription lighting_pass({}, vk::PipelineBindPoint::eGraphics, 3, lighting_input_refs, 1, &lighting_color_ref);

    const vk::SubpassDescription ui_pass({}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &lighting_color_ref);

    std::array sub_passes = { gbuffer_pass, lighting_pass, ui_pass };
    std::array<vk::SubpassDependency, 3> dependencies;

    dependencies[0] = vk::SubpassDependency(
        VK_SUBPASS_EXTERNAL,                                      // Producer: Operations before the render pass
        0,                                                        // Consumer: G-buffer pass
        vk::PipelineStageFlagBits::eBottomOfPipe,                 // Producer Stage: Wait for everything before
        vk::PipelineStageFlagBits::eColorAttachmentOutput |       // Consumer Stage: Wait until ready to write color/depth
        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::AccessFlagBits::eMemoryRead,                          // Producer Access: ...
        vk::AccessFlagBits::eColorAttachmentWrite |               // Consumer Access: Allow writing to color and depth
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::DependencyFlagBits::eByRegion
    );

    // Dependency 2: G-buffer pass (0) -> Lighting pass (1)
    // Ensures G-buffer writes are finished before the lighting pass reads them as textures.
    dependencies[1] = vk::SubpassDependency(
        0,                                                        // Producer: G-buffer pass
        1,                                                        // Consumer: Lighting pass
        vk::PipelineStageFlagBits::eColorAttachmentOutput |       // Producer Stage: Where G-buffer (color+depth) is written
        vk::PipelineStageFlagBits::eLateFragmentTests,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | 
        vk::PipelineStageFlagBits::eFragmentShader,               // Consumer Stage: Where G-buffer is read
        vk::AccessFlagBits::eColorAttachmentWrite |               // Producer Access: Make color/depth writes available
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits::eShaderRead | 
        vk::AccessFlagBits::eColorAttachmentWrite,                // Consumer Access: Allow reading from shaders
        vk::DependencyFlagBits::eByRegion
    );

    // Dependency 3: Lighting pass (1) -> UI pass (2)
    // Ensures the lighting pass finishes writing to the final color buffer before the UI pass draws on top.
    dependencies[2] = vk::SubpassDependency(
        1,                                                        // Producer: Lighting pass
        2,                                                        // Consumer: UI pass
        vk::PipelineStageFlagBits::eColorAttachmentOutput,        // Producer Stage: Where lighting output is written
        vk::PipelineStageFlagBits::eColorAttachmentOutput,        // Consumer Stage: Where UI is written
        vk::AccessFlagBits::eColorAttachmentWrite,                // Producer Access: Make lighting output available
        vk::AccessFlagBits::eColorAttachmentWrite,                // Consumer Access: Allow writing UI
        vk::DependencyFlagBits::eByRegion
    );

    const vk::RenderPassCreateInfo rp_info({}, attachments, sub_passes, dependencies);
    auto render_pass = device_data.device.createRenderPass(rp_info);

    std::vector<vk::raii::Framebuffer> frame_buffers;
    frame_buffers.reserve(image_views.size());
    for (const auto& view : image_views) {
        std::array fb_attachments = { *position_image.view, *normal_image.view, *albedo_image.view, *depth_image.view, *view };
        vk::FramebufferCreateInfo fb_info({}, *render_pass, fb_attachments, extent.width, extent.height, 1);
        frame_buffers.emplace_back(device_data.device, fb_info);
    }

    return config::swap_chain_config(
        std::move(swap_chain), surface_format, present_mode, extent, std::move(frame_buffers),
        std::move(images), std::move(image_views), format, std::move(details), std::move(render_pass),
        std::move(position_image), std::move(normal_image), std::move(albedo_image), std::move(depth_image)
    );
}