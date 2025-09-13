module;

#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform.vulkan:context;

import std;

import :config;
import :persistent_allocator;

import gse.assert;

export namespace gse::vulkan {
    auto generate_config(
        GLFWwindow* window
    ) -> std::unique_ptr<config>;

    struct frame_params {
        GLFWwindow* window;
        bool frame_buffer_resized;
        bool minimized;
		config& config;
		std::function<void()> in_frame;
    };

    auto frame(
		const frame_params& params
	) -> void;

    auto find_queue_families(
        const vk::raii::PhysicalDevice& device,
        const vk::raii::SurfaceKHR& surface
    ) -> queue_family;

	auto render(
		config& config, const vk::RenderingInfo& begin_info, const std::function<void()>& render
	) -> void;
}

namespace gse::vulkan {
    auto create_instance_and_surface(
        GLFWwindow* window
    ) -> instance_config;

    auto create_device_and_queues(
        const instance_config& instance_data
    ) -> std::tuple<device_config, queue_config>;

    auto create_command_objects(
	    const device_config& device_data,
	    const instance_config& instance_data
    ) -> command_config;

    auto create_descriptor_pool(
        const device_config& device_data
    ) -> descriptor_config;

    auto create_sync_objects(
	    const device_config& device_data,
		const swap_chain_config& swap_chain_data
    ) -> sync_config;

    auto create_swap_chain_resources(
        GLFWwindow* window,
        const instance_config& instance_data,
        const device_config& device_data
    ) -> swap_chain_config;


    vk::DebugUtilsMessengerEXT debug_utils_messenger;
    constexpr std::uint32_t max_frames_in_flight = 2;
}

auto gse::vulkan::generate_config(GLFWwindow* window) -> std::unique_ptr<config> {
    auto instance_data = create_instance_and_surface(window);
    auto [device_data, queue] = create_device_and_queues(instance_data);
    auto descriptor = create_descriptor_pool(device_data);
    auto swap_chain_data = create_swap_chain_resources(window, instance_data, device_data);
    auto command = create_command_objects(device_data, instance_data);
    auto sync = create_sync_objects(device_data, swap_chain_data);

    frame_context_config frame_context(
        0,
        *command.buffers[0]
    );

    return std::make_unique<config>(
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

auto gse::vulkan::frame(const frame_params& params) -> void {
    const auto& device = params.config.device_config().device;

    auto recreate_resources = [&] {
        device.waitIdle();
        params.config.swap_chain_config().swap_chain = nullptr;
        params.config.swap_chain_config() = create_swap_chain_resources(params.window, params.config.instance_config(), params.config.device_config());
        params.config.sync_config() = create_sync_objects(params.config.device_config(), params.config.swap_chain_config());
        };

    if (params.minimized) {
        return;
    }

    assert(
        device.waitForFences(
            *params.config.sync_config().in_flight_fences[params.config.current_frame()],
            vk::True,
            std::numeric_limits<std::uint64_t>::max()
        ) == vk::Result::eSuccess,
        "Failed to wait for in-flight fence!"
    );

    params.config.cleanup_finished_frame_resources();

    if (params.frame_buffer_resized) {
        recreate_resources();
        return;
    }

    vk::Result result;
    std::uint32_t image_index;

    try {
        const vk::AcquireNextImageInfoKHR acquire_info{
            .swapchain = *params.config.swap_chain_config().swap_chain,
            .timeout = std::numeric_limits<std::uint64_t>::max(),
            .semaphore = *params.config.sync_config().image_available_semaphores[params.config.current_frame()],
            .fence = nullptr,
            .deviceMask = 1
        };
        std::tie(result, image_index) = device.acquireNextImage2KHR(acquire_info);
    }
    catch (const vk::OutOfDateKHRError&) {
        result = vk::Result::eErrorOutOfDateKHR;
    }

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreate_resources();
        return;
    }

    assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Failed to acquire swap chain image!");

    device.resetFences(*params.config.sync_config().in_flight_fences[params.config.current_frame()]);

    params.config.frame_context() = {
        image_index,
        *params.config.command_config().buffers[params.config.current_frame()]
    };

    params.config.frame_context().command_buffer.reset({});

    constexpr vk::CommandBufferBeginInfo begin_info{
        .flags = {},
        .pInheritanceInfo = nullptr
    };

    params.config.frame_context().command_buffer.begin(begin_info);
    params.in_frame();
    params.config.frame_context().command_buffer.end();

    const vk::SemaphoreSubmitInfo wait_info{
        .semaphore = *params.config.sync_config().image_available_semaphores[params.config.current_frame()],
        .value = 0,
        .stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .deviceIndex = 0
    };

    const vk::CommandBufferSubmitInfo cmd_info{
        .commandBuffer = params.config.frame_context().command_buffer,
        .deviceMask = 1
    };

    const vk::SemaphoreSubmitInfo signal_info{
        .semaphore = *params.config.sync_config().render_finished_semaphores[image_index],
        .value = 0,
        .stageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .deviceIndex = 0
    };

    const vk::SubmitInfo2 submit_info2{
        .flags = {},
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_info
    };

    params.config.queue_config().graphics.submit2(
        submit_info2,
        *params.config.sync_config().in_flight_fences[params.config.current_frame()]
    );

    const vk::Semaphore render_finished_handle = *params.config.sync_config().render_finished_semaphores[image_index];

    const vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished_handle,
        .swapchainCount = 1,
        .pSwapchains = &*params.config.swap_chain_config().swap_chain,
        .pImageIndices = &image_index
    };

    if (const vk::Result present_result = params.config.queue_config().present.presentKHR(present_info);
        present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        recreate_resources();
    }
    else {
        assert(
            present_result == vk::Result::eSuccess,
            "Failed to present swap chain image!"
        );
    }

    params.config.current_frame() = (params.config.current_frame() + 1) % max_frames_in_flight;
}

auto gse::vulkan::create_instance_and_surface(GLFWwindow* window) -> instance_config {
    const std::vector validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

    const std::uint32_t highest_supported_version = vk::enumerateInstanceVersion();
    const vk::ApplicationInfo app_info{
        .pApplicationName = "GSEngine",
        .applicationVersion = 1,
        .pEngineName = "GSEngine",
        .engineVersion = 1,
        .apiVersion = highest_supported_version
    };

    std::uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto debug_callback = [](
        const vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
        vk::DebugUtilsMessageTypeFlagsEXT message_type,
        const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
        ) -> vk::Bool32 {
            if (message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) return VK_FALSE;

            if (std::string_view(callback_data->pMessage).find("VK_EXT_debug_utils") != std::string_view::npos) {
                return vk::False;
            }

            std::println(
                "[{}] {}\n",
                message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ? "ERROR" :
                message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ? "WARN" :
                message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo ? "INFO" : "VERB",
                callback_data->pMessage
            );

            return vk::False;
        };

    const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
        .flags = {},
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debug_callback
    };

    constexpr vk::ValidationFeatureEnableEXT enables[] = {
        vk::ValidationFeatureEnableEXT::eBestPractices,
        vk::ValidationFeatureEnableEXT::eGpuAssisted,
        vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation
    };

    const vk::ValidationFeaturesEXT features{
        .pNext = &debug_create_info,
        .enabledValidationFeatureCount = static_cast<uint32_t>(std::size(enables)),
        .pEnabledValidationFeatures = enables,
        .disabledValidationFeatureCount = 0,
        .pDisabledValidationFeatures = nullptr
    };

    const vk::InstanceCreateInfo create_info{
        .pNext = &features,
        .flags = {},
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

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
        debug_utils_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
        std::cout << "Debug Messenger Created Successfully!\n";
    }
    catch (vk::SystemError& err) {
        std::cerr << "Failed to create Debug Messenger: " << err.what() << "\n";
    }

    VkSurfaceKHR temp_surface;
    assert(glfwCreateWindowSurface(*instance, window, nullptr, &temp_surface) == VK_SUCCESS, "Error creating window surface");

    vk::raii::SurfaceKHR surface(instance, temp_surface);

    return instance_config(std::move(context), std::move(instance), std::move(surface));
}

auto gse::vulkan::create_device_and_queues(const instance_config& instance_data) -> std::tuple<device_config, queue_config> {
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
    for (std::uint32_t queue_family_index : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info{
            .flags = {},
            .queueFamilyIndex = queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceVulkan13Features vulkan13_features{
        .synchronization2 = true,
        .dynamicRendering = true
    };

    vk::PhysicalDeviceVulkan12Features vulkan12_features{
        .pNext = &vulkan13_features,
        .timelineSemaphore = true,
		.bufferDeviceAddress = true
    };

    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{
        .pNext = &vulkan12_features,
	};

    vk::PhysicalDeviceFeatures2 features2{
        .pNext = &present_wait_features,
        .features = {
			.samplerAnisotropy = true
        }
    };

    const std::vector device_extensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRDynamicRenderingExtensionName,
        vk::KHRPushDescriptorExtensionName,
    };

    vk::DeviceCreateInfo create_info{
        .pNext = &features2,
        .flags = {},
        .queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data()
    };

    vk::raii::Device device = physical_device.createDevice(create_info);

#if VK_HPP_DISPATCH_LOADER_DYNAMIC == 1
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
#endif

    std::cout << "Logical Device Created Successfully!\n";

    vk::raii::Queue graphics_queue = device.getQueue(graphics_family.value(), 0);
    vk::raii::Queue present_queue = device.getQueue(present_family.value(), 0);

    auto device_conf = device_config(std::move(physical_device), std::move(device));
    auto queue_conf = queue_config(std::move(graphics_queue), std::move(present_queue));

    return std::make_tuple(std::move(device_conf), std::move(queue_conf));
}

auto gse::vulkan::create_command_objects(const device_config& device_data, const instance_config& instance_data) -> command_config {
    const auto [graphics_family, present_family] = find_queue_families(device_data.physical_device, instance_data.surface);

    const vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphics_family.value()
    };

    vk::raii::CommandPool pool = device_data.device.createCommandPool(pool_info);

    std::cout << "Command Pool Created Successfully!\n";

    const std::string pool_name = "Primary Command Pool";
    const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
        .objectType = vk::ObjectType::eCommandPool,
        .objectHandle = reinterpret_cast<uint64_t>(static_cast<VkCommandPool>(*pool)),
        .pObjectName = pool_name.c_str()
    };
    device_data.device.setDebugUtilsObjectNameEXT(pool_name_info);

    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = max_frames_in_flight
    };

    std::vector<vk::raii::CommandBuffer> buffers = device_data.device.allocateCommandBuffers(alloc_info);

    std::cout << "Command Buffers Created Successfully!\n";

    for (std::uint32_t i = 0; i < buffers.size(); ++i) {
        const std::string name = "Primary Command Buffer " + std::to_string(i);
        const vk::DebugUtilsObjectNameInfoEXT name_info{
            .objectType = vk::ObjectType::eCommandBuffer,
            .objectHandle = reinterpret_cast<uint64_t>(static_cast<VkCommandBuffer>(*buffers[i])),
            .pObjectName = name.c_str()
        };
        device_data.device.setDebugUtilsObjectNameEXT(name_info);
    }

    return command_config(std::move(pool), std::move(buffers));
}

auto gse::vulkan::create_descriptor_pool(const device_config& device_data) -> descriptor_config {
    constexpr uint32_t max_sets = 512;

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eUniformBuffer,          4096 },
        { vk::DescriptorType::eCombinedImageSampler,   4096 },
        { vk::DescriptorType::eStorageBuffer,          1024 },
        { vk::DescriptorType::eInputAttachment,        4096 },
    };

    const vk::DescriptorPoolCreateInfo pool_info{
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data()
    };

    vk::raii::DescriptorPool pool = device_data.device.createDescriptorPool(pool_info);

    std::cout << "Descriptor Pool Created Successfully!\n";

    return descriptor_config(std::move(pool));
}

auto gse::vulkan::create_sync_objects(const device_config& device_data, const swap_chain_config& swap_chain_data) -> sync_config {
    std::vector<vk::raii::Semaphore> image_available;
    std::vector<vk::raii::Semaphore> render_finished;
    std::vector<vk::raii::Fence> in_flight_fences;

    const auto swap_chain_image_count = swap_chain_data.images.size();
    image_available.reserve(swap_chain_image_count);
    render_finished.reserve(swap_chain_image_count);

    in_flight_fences.reserve(max_frames_in_flight);

    constexpr vk::SemaphoreCreateInfo bin_sem_ci{};
    constexpr vk::FenceCreateInfo fence_ci{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    for (std::size_t i = 0; i < swap_chain_image_count; ++i) {
        image_available.emplace_back(device_data.device, bin_sem_ci);
        render_finished.emplace_back(device_data.device, bin_sem_ci);
    }

    for (std::size_t i = 0; i < max_frames_in_flight; ++i) {
        in_flight_fences.emplace_back(device_data.device, fence_ci);
    }

    return sync_config(
        std::move(image_available),
        std::move(render_finished),
        std::move(in_flight_fences)
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

auto gse::vulkan::render(config& config, const vk::RenderingInfo& begin_info, const std::function<void()>& render) -> void {
    config.frame_context().command_buffer.beginRendering(begin_info);
    render();
    config.frame_context().command_buffer.endRendering();
}

auto gse::vulkan::create_swap_chain_resources(GLFWwindow* window, const instance_config& instance_data, const device_config& device_data) -> swap_chain_config {
    swap_chain_details details = {
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
    }
    else {
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

    vk::SwapchainCreateInfoKHR create_info{
        .flags = {},
        .surface = *instance_data.surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
    };

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

    auto normal_image = persistent_allocator::create_image(
        device_data, 
        {
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8G8Snorm,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
        }, 
        vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
            .flags = {},
	        .image = nullptr,
	        .viewType = vk::ImageViewType::e2D,
	        .format = vk::Format::eR8G8Snorm,
	        .components = {},
	        .subresourceRange = {
	            .aspectMask = vk::ImageAspectFlagBits::eColor,
	            .baseMipLevel = 0,
	            .levelCount = 1,
	            .baseArrayLayer = 0,
	            .layerCount = 1
	        }
		}
    );

    auto albedo_image = persistent_allocator::create_image(
        device_data, 
        {
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eB10G11R11UfloatPack32,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
        }, 
        vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
            .flags = {},
	        .image = nullptr,
	        .viewType = vk::ImageViewType::e2D,
	        .format = vk::Format::eB10G11R11UfloatPack32,
	        .components = {},
	        .subresourceRange = {
	            .aspectMask = vk::ImageAspectFlagBits::eColor,
	            .baseMipLevel = 0,
	            .levelCount = 1,
	            .baseArrayLayer = 0,
	            .layerCount = 1
	        }
		}
    );

    auto depth_image = persistent_allocator::create_image(
        device_data, 
        {
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eD32Sfloat,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
        }, 
        vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
            .flags = {},
	        .image = nullptr,
	        .viewType = vk::ImageViewType::e2D,
	        .format = vk::Format::eD32Sfloat,
	        .components = {},
	        .subresourceRange = {
	            .aspectMask = vk::ImageAspectFlagBits::eDepth,
	            .baseMipLevel = 0,
	            .levelCount = 1,
	            .baseArrayLayer = 0,
	            .layerCount = 1
	        }
		}
    );

    std::vector<vk::raii::ImageView> image_views;
    image_views.reserve(images.size());
    for (const auto& image : images) {
        vk::ImageViewCreateInfo iv_create_info{
            .flags = {},
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components = {},
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        image_views.emplace_back(device_data.device, iv_create_info);
    }

    return swap_chain_config(
        std::move(swap_chain), surface_format, present_mode, extent,
        std::move(images), std::move(image_views), format, std::move(details),
        std::move(normal_image), std::move(albedo_image), std::move(depth_image)
    );
}