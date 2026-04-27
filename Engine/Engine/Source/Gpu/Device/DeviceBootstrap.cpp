module gse.gpu.vulkan;

import std;
import vulkan;

import :device_bootstrap;
import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;
import :descriptor_buffer_types;
import :gpu_command_pools;

import gse.os;
import gse.assert;
import gse.log;
import gse.config;
import gse.concurrency;
import gse.save;

namespace gse::vulkan {
    inline vk::DebugUtilsMessengerEXT debug_utils_messenger;

    struct device_creation_result {
        device_config device;
        queue_config queue;
        bool video_encode_enabled = false;
        descriptor_buffer_properties desc_buf_props;
    };

    auto query_descriptor_buffer_properties(
        const vk::raii::PhysicalDevice& physical_device
    ) -> descriptor_buffer_properties;

    auto create_instance(
        std::span<const char* const> required_extensions
    ) -> instance_config;

    auto create_device_and_queues(
        const instance_config& instance_data
    ) -> device_creation_result;

    auto create_command_objects(
        const device_config& device_data,
        const instance_config& instance_data
    ) -> command_config;

    auto pick_surface_format(
        const vk::raii::PhysicalDevice& physical_device,
        const vk::raii::SurfaceKHR& surface
    ) -> vk::Format;
}

auto gse::vulkan::find_queue_families(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> queue_family {
    queue_family indices;
    const auto queue_families = device.getQueueFamilyProperties();
    for (std::uint32_t i = 0; i < queue_families.size(); i++) {
        log::println(log::category::vulkan, "Queue family {}: flags = {}", i, vk::to_string(queue_families[i].queueFlags));
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }
        if (device.getSurfaceSupportKHR(i, surface)) {
            indices.present_family = i;
        }
        if ((queue_families[i].queueFlags & vk::QueueFlagBits::eCompute) &&
            !(queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
            indices.compute_family = i;
        }
        if ((queue_families[i].queueFlags & vk::QueueFlagBits::eVideoEncodeKHR) &&
            !indices.video_encode_family.has_value()) {
            indices.video_encode_family = i;
        }
    }
    if (!indices.compute_family.has_value()) {
        indices.compute_family = indices.graphics_family;
    }
    return indices;
}

auto gse::vulkan::query_descriptor_buffer_properties(const vk::raii::PhysicalDevice& physical_device) -> descriptor_buffer_properties {
    const auto props = physical_device.getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceDescriptorBufferPropertiesEXT
    >();
    const auto& db = props.get<vk::PhysicalDeviceDescriptorBufferPropertiesEXT>();

    log::println(log::category::vulkan, "Descriptor buffer properties:");
    log::println(log::category::vulkan, "  offset alignment: {}", db.descriptorBufferOffsetAlignment);
    log::println(log::category::vulkan, "  uniform buffer size: {}", db.uniformBufferDescriptorSize);
    log::println(log::category::vulkan, "  storage buffer size: {}", db.storageBufferDescriptorSize);
    log::println(log::category::vulkan, "  sampled image size: {}", db.sampledImageDescriptorSize);
    log::println(log::category::vulkan, "  sampler size: {}", db.samplerDescriptorSize);
    log::println(log::category::vulkan, "  combined image sampler size: {}", db.combinedImageSamplerDescriptorSize);
    log::println(log::category::vulkan, "  storage image size: {}", db.storageImageDescriptorSize);
    log::println(log::category::vulkan, "  input attachment size: {}", db.inputAttachmentDescriptorSize);
    log::println(log::category::vulkan, "  bufferless push descriptors: {}", db.bufferlessPushDescriptors ? "true" : "false");
    log::println(log::category::vulkan, "  acceleration structure size: {}", db.accelerationStructureDescriptorSize);

    return {
        .offset_alignment = db.descriptorBufferOffsetAlignment,
        .uniform_buffer_descriptor_size = db.uniformBufferDescriptorSize,
        .storage_buffer_descriptor_size = db.storageBufferDescriptorSize,
        .sampled_image_descriptor_size = db.sampledImageDescriptorSize,
        .sampler_descriptor_size = db.samplerDescriptorSize,
        .combined_image_sampler_descriptor_size = db.combinedImageSamplerDescriptorSize,
        .storage_image_descriptor_size = db.storageImageDescriptorSize,
        .input_attachment_descriptor_size = db.inputAttachmentDescriptorSize,
        .acceleration_structure_descriptor_size = db.accelerationStructureDescriptorSize,
        .push_descriptors_supported = false,
        .bufferless_push_descriptors = static_cast<bool>(db.bufferlessPushDescriptors),
    };
}

auto gse::vulkan::create_instance(const std::span<const char* const> required_extensions) -> instance_config {
    const bool enable_validation = save::read_bool_setting_early(
        config::resource_path / "Misc/settings.toml",
        "Graphics",
        "Validation Layers",
        true
    );

    std::vector<const char*> validation_layers;
    if (enable_validation) {
        validation_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    vk::detail::defaultDispatchLoaderDynamic.init();

    const std::uint32_t highest_supported_version = vk::enumerateInstanceVersion();
    const vk::ApplicationInfo app_info{
        .pApplicationName = "GSEngine",
        .applicationVersion = 1,
        .pEngineName = "GSEngine",
        .engineVersion = 1,
        .apiVersion = highest_supported_version,
    };

    std::vector extensions(required_extensions.begin(), required_extensions.end());
    extensions.push_back(vk::EXTDebugUtilsExtensionName);

    auto debug_callback = [](
        const vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
        vk::DebugUtilsMessageTypeFlagsEXT message_type,
        const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
    ) -> vk::Bool32 {
        if (message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
            return vk::False;
        }

        if (std::string_view(callback_data->pMessage).find("VK_EXT_debug_utils") != std::string_view::npos) {
            return vk::False;
        }

        const auto lvl =
            message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ? log::level::error :
            message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ? log::level::warning :
            log::level::info;

        log::println(lvl, log::category::vulkan_validation, "{}", callback_data->pMessage);

        for (std::uint32_t i = 0; i < callback_data->objectCount; ++i) {
            const auto& object = callback_data->pObjects[i];
            if (!object.pObjectName || object.pObjectName[0] == '\0') {
                continue;
            }

            log::println(
                lvl,
                log::category::vulkan_validation,
                "Object {}: {} 0x{:x} '{}'",
                i,
                vk::to_string(object.objectType),
                object.objectHandle,
                object.pObjectName
            );
        }

        log::flush();

        return vk::False;
    };

    const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
        .flags = {},
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debug_callback,
    };

    constexpr vk::ValidationFeatureEnableEXT enables[] = {
        vk::ValidationFeatureEnableEXT::eBestPractices,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
    };

    const vk::ValidationFeaturesEXT features{
        .pNext = &debug_create_info,
        .enabledValidationFeatureCount = static_cast<std::uint32_t>(std::size(enables)),
        .pEnabledValidationFeatures = enables,
        .disabledValidationFeatureCount = 0,
        .pDisabledValidationFeatures = nullptr,
    };

    const vk::InstanceCreateInfo create_info{
        .pNext = enable_validation ? static_cast<const void*>(&features) : nullptr,
        .flags = {},
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<std::uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.empty() ? nullptr : validation_layers.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    vk::raii::Instance instance = nullptr;
    vk::raii::Context context;

    try {
        instance = vk::raii::Instance(context, create_info);
        log::println(log::category::vulkan, "Vulkan Instance Created{}!", enable_validation ? " with validation layers" : "");
    } catch (vk::SystemError& err) {
        log::println(log::level::error, log::category::vulkan, "Failed to create Vulkan instance: {}", err.what());
        throw;
    }

    vk::detail::defaultDispatchLoaderDynamic.init(*instance);

    if (enable_validation) {
        try {
            debug_utils_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
            log::println(log::category::vulkan, "Debug Messenger Created Successfully!");
        } catch (vk::SystemError& err) {
            log::println(log::level::error, log::category::vulkan, "Failed to create Debug Messenger: {}", err.what());
        }
    }

    return instance_config(std::move(context), std::move(instance), nullptr);
}

auto gse::vulkan::create_device_and_queues(const instance_config& instance_data) -> device_creation_result {
    const auto devices = instance_data.instance.enumeratePhysicalDevices();
    assert(!devices.empty(), std::source_location::current(), "No Vulkan-compatible GPUs found!");

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

    const auto physical_device_properties = physical_device.getProperties();
    log::println(log::category::vulkan, "Selected GPU: {}", std::string_view(physical_device_properties.deviceName.data()));

    auto [graphics_family, present_family, compute_family, video_encode_family] = find_queue_families(physical_device, instance_data.surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set unique_queue_families = {
        graphics_family.value(),
        present_family.value(),
        compute_family.value(),
    };

    if (video_encode_family.has_value()) {
        unique_queue_families.insert(video_encode_family.value());
    }

    float queue_priority = 1.0f;
    for (std::uint32_t queue_family_index : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info{
            .flags = {},
            .queueFamilyIndex = queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
        queue_create_infos.push_back(queue_create_info);
    }

    const auto available_extensions = physical_device.enumerateDeviceExtensionProperties();
    const auto supports_extension = [&](const char* extension_name) {
        return std::ranges::any_of(available_extensions, [&](const auto& extension) {
            return std::strcmp(extension.extensionName.data(), extension_name) == 0;
        });
    };
    const bool device_fault_extension_supported = supports_extension(vk::EXTDeviceFaultExtensionName);
    const bool robustness2_extension_supported = supports_extension(vk::EXTRobustness2ExtensionName);

    const bool rt_extensions_available =
        supports_extension(vk::KHRDeferredHostOperationsExtensionName) &&
        supports_extension(vk::KHRAccelerationStructureExtensionName) &&
        supports_extension(vk::KHRRayQueryExtensionName);

    const bool video_encode_extensions_available =
        video_encode_family.has_value() &&
        supports_extension(vk::KHRVideoQueueExtensionName) &&
        supports_extension(vk::KHRVideoEncodeQueueExtensionName);

    const auto feature_chain = physical_device.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceMeshShaderFeaturesEXT,
        vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
        vk::PhysicalDeviceFaultFeaturesEXT,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayQueryFeaturesKHR,
        vk::PhysicalDeviceRobustness2FeaturesEXT
    >();
    const auto& mesh_shader_query = feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
    assert(
        mesh_shader_query.meshShader && mesh_shader_query.taskShader,
        std::source_location::current(),
        "Mesh shaders are required but not supported by this GPU"
    );
    const auto& as_query = feature_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    const auto& rq_query = feature_chain.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
    assert(
        rt_extensions_available && as_query.accelerationStructure && rq_query.rayQuery,
        std::source_location::current(),
        "Ray tracing (acceleration structure + ray query) is required but not supported by this GPU"
    );
    const auto& desc_buf_query = feature_chain.get<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>();
    assert(
        desc_buf_query.descriptorBuffer,
        std::source_location::current(),
        "Descriptor buffer is required but not supported by this GPU"
    );
    const bool descriptor_buffer_push_descriptors_supported = desc_buf_query.descriptorBufferPushDescriptors;
    const auto& fault_query = feature_chain.get<vk::PhysicalDeviceFaultFeaturesEXT>();
    const bool device_fault_supported = device_fault_extension_supported && fault_query.deviceFault;
    const bool device_fault_vendor_binary_supported = device_fault_supported && fault_query.deviceFaultVendorBinary;
    const auto& robustness2_query = feature_chain.get<vk::PhysicalDeviceRobustness2FeaturesEXT>();
    const bool robustness2_supported = robustness2_extension_supported && robustness2_query.robustBufferAccess2;

    log::println(log::category::vulkan, "Mesh shader support detected");
    log::println(log::category::vulkan, "Ray tracing support detected");
    log::println(log::category::vulkan, "Descriptor buffer support detected");

    if (descriptor_buffer_push_descriptors_supported) {
        log::println(log::category::vulkan, "Descriptor buffer push descriptor interop detected");
    }

    if (device_fault_supported) {
        log::println(log::category::vulkan, "Device fault support detected");
    }

    if (robustness2_supported) {
        log::println(log::category::vulkan, "Robustness2 support detected (robustBufferAccess2)");
    } else {
        log::println(log::level::warning, log::category::vulkan, "VK_EXT_robustness2 not supported");
    }

    vk::PhysicalDeviceMemoryPriorityFeaturesEXT memory_priority_features{
        .memoryPriority = vk::True,
    };

    vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_memory_features{
        .pNext = &memory_priority_features,
        .pageableDeviceLocalMemory = vk::True,
    };

    vk::PhysicalDeviceMaintenance6FeaturesKHR maintenance6_features{
        .pNext = &pageable_memory_features,
        .maintenance6 = vk::True,
    };

    vk::PhysicalDeviceMaintenance5FeaturesKHR maintenance5_features{
        .pNext = &maintenance6_features,
        .maintenance5 = vk::True,
    };

    vk::PhysicalDeviceVulkan13Features vulkan13_features{
        .pNext = &maintenance5_features,
        .synchronization2 = vk::True,
        .dynamicRendering = vk::True,
    };

    vk::PhysicalDeviceVulkan12Features vulkan12_features{
        .pNext = &vulkan13_features,
        .shaderSampledImageArrayNonUniformIndexing = vk::True,
        .descriptorBindingSampledImageUpdateAfterBind = vk::True,
        .descriptorBindingStorageImageUpdateAfterBind = vk::True,
        .descriptorBindingStorageBufferUpdateAfterBind = vk::True,
        .descriptorBindingPartiallyBound = vk::True,
        .descriptorBindingVariableDescriptorCount = vk::True,
        .runtimeDescriptorArray = vk::True,
        .scalarBlockLayout = vk::True,
        .hostQueryReset = vk::True,
        .timelineSemaphore = vk::True,
        .bufferDeviceAddress = vk::True,
    };

    vk::PhysicalDeviceVulkan11Features vulkan11_features{
        .pNext = &vulkan12_features,
        .shaderDrawParameters = vk::True,
    };

    vk::PhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
        .pNext = &vulkan11_features,
        .taskShader = vk::True,
        .meshShader = vk::True,
    };

    vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{
        .pNext = &mesh_shader_features,
        .descriptorBuffer = vk::True,
        .descriptorBufferPushDescriptors = descriptor_buffer_push_descriptors_supported ? vk::True : vk::False,
    };

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features{
        .pNext = &descriptor_buffer_features,
        .accelerationStructure = vk::True,
    };

    vk::PhysicalDeviceRayQueryFeaturesKHR ray_query_features{
        .pNext = &as_features,
        .rayQuery = vk::True,
    };

    vk::PhysicalDeviceFaultFeaturesEXT fault_features{
        .pNext = &ray_query_features,
        .deviceFault = device_fault_supported ? vk::True : vk::False,
        .deviceFaultVendorBinary = device_fault_vendor_binary_supported ? vk::True : vk::False,
    };

    void* post_fault_chain_head = device_fault_supported ? static_cast<void*>(&fault_features) : static_cast<void*>(&ray_query_features);

    vk::PhysicalDeviceRobustness2FeaturesEXT robustness2_features{
        .pNext = post_fault_chain_head,
        .robustBufferAccess2 = robustness2_supported ? vk::True : vk::False,
        .robustImageAccess2 = robustness2_supported ? vk::True : vk::False,
        .nullDescriptor = robustness2_supported ? vk::True : vk::False,
    };

    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{
        .pNext = robustness2_supported ? static_cast<void*>(&robustness2_features) : post_fault_chain_head,
    };

    const bool av1_encode_supported = video_encode_extensions_available && supports_extension(vk::KHRVideoEncodeAv1ExtensionName);

    vk::PhysicalDeviceVideoEncodeAV1FeaturesKHR av1_encode_features{
        .pNext = &present_wait_features,
        .videoEncodeAV1 = vk::True,
    };

    void* post_video_chain_head = av1_encode_supported
        ? static_cast<void*>(&av1_encode_features)
        : static_cast<void*>(&present_wait_features);

    vk::PhysicalDeviceFeatures2 features2{
        .pNext = post_video_chain_head,
        .features = {
            .robustBufferAccess = robustness2_supported ? vk::True : vk::False,
            .drawIndirectFirstInstance = vk::True,
            .fillModeNonSolid = vk::True,
            .samplerAnisotropy = vk::True,
            .pipelineStatisticsQuery = vk::True,
        },
    };

    std::vector device_extensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRDynamicRenderingExtensionName,
        vk::KHRPushDescriptorExtensionName,
        vk::KHRMaintenance5ExtensionName,
        vk::KHRMaintenance6ExtensionName,
        vk::EXTCalibratedTimestampsExtensionName,
        vk::EXTPageableDeviceLocalMemoryExtensionName,
        vk::EXTMemoryPriorityExtensionName,
        vk::EXTMeshShaderExtensionName,
        vk::EXTDescriptorBufferExtensionName,
        vk::KHRDeferredHostOperationsExtensionName,
        vk::KHRAccelerationStructureExtensionName,
        vk::KHRRayQueryExtensionName,
    };

    if (device_fault_supported) {
        device_extensions.push_back(vk::EXTDeviceFaultExtensionName);
    }

    if (robustness2_supported) {
        device_extensions.push_back(vk::EXTRobustness2ExtensionName);
    }

    if (video_encode_extensions_available) {
        device_extensions.push_back(vk::KHRVideoQueueExtensionName);
        device_extensions.push_back(vk::KHRVideoEncodeQueueExtensionName);
        if (supports_extension(vk::KHRVideoEncodeAv1ExtensionName)) {
            device_extensions.push_back(vk::KHRVideoEncodeAv1ExtensionName);
        }
        if (supports_extension(vk::KHRVideoEncodeH265ExtensionName)) {
            device_extensions.push_back(vk::KHRVideoEncodeH265ExtensionName);
        }
    }

    vk::DeviceCreateInfo create_info{
        .pNext = &features2,
        .flags = {},
        .queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    vk::raii::Device device = physical_device.createDevice(create_info);

    vk::detail::defaultDispatchLoaderDynamic.init(*device);

    log::println(log::category::vulkan, "Logical Device Created Successfully!");

    vk::raii::Queue graphics_queue = device.getQueue(graphics_family.value(), 0);
    vk::raii::Queue present_queue = device.getQueue(present_family.value(), 0);
    vk::raii::Queue compute_queue = device.getQueue(compute_family.value(), 0);

    auto desc_buf_props = query_descriptor_buffer_properties(physical_device);
    desc_buf_props.push_descriptors_supported = descriptor_buffer_push_descriptors_supported;

    auto result = device_creation_result{
        .device = device_config(
            std::move(physical_device),
            std::move(device),
            device_fault_supported,
            device_fault_vendor_binary_supported
        ),
        .queue = queue_config(std::move(graphics_queue), std::move(present_queue), std::move(compute_queue), compute_family.value()),
        .video_encode_enabled = video_encode_extensions_available,
        .desc_buf_props = std::move(desc_buf_props),
    };

    if (video_encode_extensions_available) {
        result.queue.set_video_encode(result.device.device.getQueue(video_encode_family.value(), 0), video_encode_family.value());
        log::println(log::category::vulkan, "Video encode queue acquired (family {})", video_encode_family.value());
    }

    return result;
}

auto gse::vulkan::create_command_objects(const device_config& device_data, const instance_config& instance_data) -> command_config {
    const auto [graphics_family, present_family, _, __] = find_queue_families(device_data.physical_device, instance_data.surface);

    const vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphics_family.value(),
    };

    vk::raii::CommandPool pool = device_data.device.createCommandPool(pool_info);

    log::println(log::category::vulkan, "Command Pool Created Successfully!");

    constexpr auto pool_name = "Primary Command Pool";
    const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
        .objectType = vk::ObjectType::eCommandPool,
        .objectHandle = std::bit_cast<std::uint64_t>(*pool),
        .pObjectName = pool_name,
    };
    device_data.device.setDebugUtilsObjectNameEXT(pool_name_info);

    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = max_frames_in_flight,
    };

    std::vector<vk::raii::CommandBuffer> buffers = device_data.device.allocateCommandBuffers(alloc_info);

    log::println(log::category::vulkan, "Command Buffers Created Successfully!");

    for (std::uint32_t i = 0; i < buffers.size(); ++i) {
        const std::string name = "Primary Command Buffer " + std::to_string(i);
        const vk::DebugUtilsObjectNameInfoEXT name_info{
            .objectType = vk::ObjectType::eCommandBuffer,
            .objectHandle = std::bit_cast<std::uint64_t>(*buffers[i]),
            .pObjectName = name.c_str(),
        };
        device_data.device.setDebugUtilsObjectNameEXT(name_info);
    }

    return command_config(std::move(pool), std::move(buffers), graphics_family.value());
}

auto gse::vulkan::pick_surface_format(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface) -> vk::Format {
    const auto formats = physical_device.getSurfaceFormatsKHR(*surface);
    for (const auto& [format, colorSpace] : formats) {
        if (format == vk::Format::eB8G8R8A8Srgb && colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    return formats[0].format;
}

auto gse::vulkan::bootstrap_device(const window& win, save::state& save) -> bootstrap_result {
    auto instance_data = create_instance(window::vulkan_instance_extensions());
    instance_data.surface = vk::raii::SurfaceKHR(instance_data.instance, win.create_vulkan_surface(*instance_data.instance));

    auto [dev_config, queue, video_encode_enabled, desc_buf_props] = create_device_and_queues(instance_data);

    auto alloc = std::make_unique<allocator>(dev_config.device, dev_config.physical_device, save);
    auto command = create_command_objects(dev_config, instance_data);

    auto worker_pools = worker_command_pools::create(
        dev_config,
        command.graphics_family,
        task::thread_count()
    );

    auto desc_heap = std::make_unique<descriptor_heap>(dev_config.device, dev_config.physical_device, desc_buf_props);

    const auto surface_format = pick_surface_format(dev_config.physical_device, instance_data.surface);

    return bootstrap_result{
        .instance = std::move(instance_data),
        .device = std::move(dev_config),
        .alloc = std::move(alloc),
        .queue = std::move(queue),
        .command = std::move(command),
        .worker_pools = std::move(worker_pools),
        .desc_heap = std::move(desc_heap),
        .desc_buf_props = std::move(desc_buf_props),
        .surface_format = surface_format,
        .video_encode_enabled = video_encode_enabled,
    };
}
