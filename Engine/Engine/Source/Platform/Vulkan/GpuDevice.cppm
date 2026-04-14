export module gse.platform:gpu_device;

import std;
import vulkan;

import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;
import :descriptor_buffer_types;
import :gpu_types;
import :gpu_image;
import :shader;
import :shader_layout;
import :window;

import gse.assert;
import gse.log;
import gse.math;
import gse.utility;

export namespace vk::detail {
	DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

export namespace gse::vulkan {
	constexpr std::uint32_t max_frames_in_flight = 2;

	struct device_creation_result {
		device_config device;
		queue_config queue;
		bool mesh_shaders_enabled = false;
		bool ray_tracing_enabled = false;
		bool video_encode_enabled = false;
		descriptor_buffer_properties desc_buf_props;
	};

	auto find_queue_families(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> queue_family;
}

namespace gse::vulkan {
	vk::DebugUtilsMessengerEXT debug_utils_messenger;

	auto find_queue_families(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> queue_family {
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

	auto query_descriptor_buffer_properties(const vk::raii::PhysicalDevice& physical_device) -> descriptor_buffer_properties {
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
			.supported = true
		};
	}

	auto create_instance(const std::span<const char* const> required_extensions) -> instance_config {
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
			.apiVersion = highest_supported_version
		};

		std::vector extensions(required_extensions.begin(), required_extensions.end());
		extensions.push_back(vk::EXTDebugUtilsExtensionName);

		auto debug_callback = [](
			const vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
			vk::DebugUtilsMessageTypeFlagsEXT message_type,
			const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
			void* user_data
		) -> vk::Bool32 {
			if (message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) return vk::False;

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
			.pfnUserCallback = debug_callback
		};

		constexpr vk::ValidationFeatureEnableEXT enables[] = {
			vk::ValidationFeatureEnableEXT::eBestPractices,
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
			.pNext = enable_validation ? static_cast<const void*>(&features) : nullptr,
			.flags = {},
			.pApplicationInfo = &app_info,
			.enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
			.ppEnabledLayerNames = validation_layers.empty() ? nullptr : validation_layers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data()
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

	auto create_device_and_queues(const instance_config& instance_data) -> device_creation_result {
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
			compute_family.value()
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
				.pQueuePriorities = &queue_priority
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
		const bool mesh_shaders_supported = mesh_shader_query.meshShader && mesh_shader_query.taskShader;
		const auto& desc_buf_query = feature_chain.get<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>();
		const bool descriptor_buffer_supported = desc_buf_query.descriptorBuffer;
		const bool descriptor_buffer_push_descriptors_supported = desc_buf_query.descriptorBufferPushDescriptors;
		const auto& fault_query = feature_chain.get<vk::PhysicalDeviceFaultFeaturesEXT>();
		const bool device_fault_supported = device_fault_extension_supported && fault_query.deviceFault;
		const bool device_fault_vendor_binary_supported = device_fault_supported && fault_query.deviceFaultVendorBinary;
		const auto& as_query = feature_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
		const auto& rq_query = feature_chain.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
		const bool ray_tracing_supported = rt_extensions_available && as_query.accelerationStructure && rq_query.rayQuery;
		const auto& robustness2_query = feature_chain.get<vk::PhysicalDeviceRobustness2FeaturesEXT>();
		const bool robustness2_supported = robustness2_extension_supported && robustness2_query.robustBufferAccess2;

		if (mesh_shaders_supported) {
			log::println(log::category::vulkan, "Mesh shader support detected");
		} else {
			log::println(log::level::warning, log::category::vulkan, "Mesh shaders not supported, using vertex pipeline fallback");
		}

		if (descriptor_buffer_supported) {
			log::println(log::category::vulkan, "Descriptor buffer support detected");
		} else {
			log::println(log::level::warning, log::category::vulkan, "Descriptor buffer not supported");
		}

		if (descriptor_buffer_supported && descriptor_buffer_push_descriptors_supported) {
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

		if (ray_tracing_supported) {
			log::println(log::category::vulkan, "Ray tracing support detected");
		} else {
			log::println(log::level::warning, log::category::vulkan, "Ray tracing not supported");
		}

		vk::PhysicalDeviceVulkan13Features vulkan13_features{
			.synchronization2 = vk::True,
			.dynamicRendering = vk::True
		};

		vk::PhysicalDeviceVulkan12Features vulkan12_features{
			.pNext = &vulkan13_features,
			.scalarBlockLayout = vk::True,
			.hostQueryReset = vk::True,
			.timelineSemaphore = vk::True,
			.bufferDeviceAddress = vk::True
		};

		vk::PhysicalDeviceVulkan11Features vulkan11_features{
			.pNext = &vulkan12_features,
			.shaderDrawParameters = vk::True
		};

		vk::PhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
			.pNext = &vulkan11_features,
			.taskShader = vk::True,
			.meshShader = vk::True
		};

		vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{
			.pNext = mesh_shaders_supported
				? static_cast<void*>(&mesh_shader_features)
				: static_cast<void*>(&vulkan11_features),
			.descriptorBuffer = vk::True,
			.descriptorBufferPushDescriptors = descriptor_buffer_push_descriptors_supported ? vk::True : vk::False
		};

		void* pre_rt_chain_head = descriptor_buffer_supported
			? static_cast<void*>(&descriptor_buffer_features)
			: (mesh_shaders_supported
				? static_cast<void*>(&mesh_shader_features)
				: static_cast<void*>(&vulkan11_features));

		vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features{
			.pNext = pre_rt_chain_head,
			.accelerationStructure = vk::True
		};

		vk::PhysicalDeviceRayQueryFeaturesKHR ray_query_features{
			.pNext = &as_features,
			.rayQuery = vk::True
		};

		void* feature_chain_head = ray_tracing_supported
			? static_cast<void*>(&ray_query_features)
			: pre_rt_chain_head;

		vk::PhysicalDeviceFaultFeaturesEXT fault_features{
			.pNext = feature_chain_head,
			.deviceFault = device_fault_supported ? vk::True : vk::False,
			.deviceFaultVendorBinary = device_fault_vendor_binary_supported ? vk::True : vk::False
		};

		void* post_fault_chain_head = device_fault_supported ? static_cast<void*>(&fault_features) : feature_chain_head;

		vk::PhysicalDeviceRobustness2FeaturesEXT robustness2_features{
			.pNext = post_fault_chain_head,
			.robustBufferAccess2 = robustness2_supported ? vk::True : vk::False,
			.robustImageAccess2 = robustness2_supported ? vk::True : vk::False,
			.nullDescriptor = robustness2_supported ? vk::True : vk::False
		};

		vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{
			.pNext = robustness2_supported ? static_cast<void*>(&robustness2_features) : post_fault_chain_head
		};

		const bool av1_encode_supported = video_encode_extensions_available && supports_extension(vk::KHRVideoEncodeAv1ExtensionName);

		vk::PhysicalDeviceVideoEncodeAV1FeaturesKHR av1_encode_features{
			.pNext = &present_wait_features,
			.videoEncodeAV1 = vk::True
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
				.samplerAnisotropy = vk::True
			}
		};

		std::vector device_extensions = {
			vk::KHRSwapchainExtensionName,
			vk::KHRSynchronization2ExtensionName,
			vk::KHRDynamicRenderingExtensionName,
			vk::KHRPushDescriptorExtensionName
		};

		if (mesh_shaders_supported) {
			device_extensions.push_back(vk::EXTMeshShaderExtensionName);
		}

		if (descriptor_buffer_supported) {
			device_extensions.push_back(vk::EXTDescriptorBufferExtensionName);
		}

		if (device_fault_supported) {
			device_extensions.push_back(vk::EXTDeviceFaultExtensionName);
		}

		if (robustness2_supported) {
			device_extensions.push_back(vk::EXTRobustness2ExtensionName);
		}

		if (ray_tracing_supported) {
			device_extensions.push_back(vk::KHRDeferredHostOperationsExtensionName);
			device_extensions.push_back(vk::KHRAccelerationStructureExtensionName);
			device_extensions.push_back(vk::KHRRayQueryExtensionName);
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
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
			.ppEnabledExtensionNames = device_extensions.data()
		};

		vk::raii::Device device = physical_device.createDevice(create_info);

		vk::detail::defaultDispatchLoaderDynamic.init(*device);

		log::println(log::category::vulkan, "Logical Device Created Successfully!");

		vk::raii::Queue graphics_queue = device.getQueue(graphics_family.value(), 0);
		vk::raii::Queue present_queue = device.getQueue(present_family.value(), 0);
		vk::raii::Queue compute_queue = device.getQueue(compute_family.value(), 0);

		auto desc_buf_props = descriptor_buffer_supported
			? query_descriptor_buffer_properties(physical_device)
			: descriptor_buffer_properties{};
		desc_buf_props.push_descriptors_supported = descriptor_buffer_push_descriptors_supported;

		auto result = device_creation_result{
			.device = device_config(
				std::move(physical_device),
				std::move(device),
				device_fault_supported,
				device_fault_vendor_binary_supported
			),
			.queue = queue_config(std::move(graphics_queue), std::move(present_queue), std::move(compute_queue), compute_family.value()),
			.mesh_shaders_enabled = mesh_shaders_supported,
			.ray_tracing_enabled = ray_tracing_supported,
			.video_encode_enabled = video_encode_extensions_available,
			.desc_buf_props = std::move(desc_buf_props)
		};

		if (video_encode_extensions_available) {
			result.queue.set_video_encode(result.device.device.getQueue(video_encode_family.value(), 0), video_encode_family.value());
			log::println(log::category::vulkan, "Video encode queue acquired (family {})", video_encode_family.value());
		}

		return result;
	}

	auto create_command_objects(const device_config& device_data, const instance_config& instance_data) -> command_config {
		const auto [graphics_family, present_family, _, __] = find_queue_families(device_data.physical_device, instance_data.surface);

		const vk::CommandPoolCreateInfo pool_info{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = graphics_family.value()
		};

		vk::raii::CommandPool pool = device_data.device.createCommandPool(pool_info);

		log::println(log::category::vulkan, "Command Pool Created Successfully!");

		constexpr auto pool_name = "Primary Command Pool";
		const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
			.objectType = vk::ObjectType::eCommandPool,
			.objectHandle = reinterpret_cast<std::uint64_t>(static_cast<VkCommandPool>(*pool)),
			.pObjectName = pool_name
		};
		device_data.device.setDebugUtilsObjectNameEXT(pool_name_info);

		const vk::CommandBufferAllocateInfo alloc_info{
			.commandPool = *pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = max_frames_in_flight
		};

		std::vector<vk::raii::CommandBuffer> buffers = device_data.device.allocateCommandBuffers(alloc_info);

		log::println(log::category::vulkan, "Command Buffers Created Successfully!");

		for (std::uint32_t i = 0; i < buffers.size(); ++i) {
			const std::string name = "Primary Command Buffer " + std::to_string(i);
			const vk::DebugUtilsObjectNameInfoEXT name_info{
				.objectType = vk::ObjectType::eCommandBuffer,
				.objectHandle = reinterpret_cast<std::uint64_t>(static_cast<VkCommandBuffer>(*buffers[i])),
				.pObjectName = name.c_str()
			};
			device_data.device.setDebugUtilsObjectNameEXT(name_info);
		}

		return command_config(std::move(pool), std::move(buffers));
	}

	auto to_vk_stage_flags(const gpu::stage_flags flags) -> vk::ShaderStageFlags {
		vk::ShaderStageFlags result;
		using enum gpu::stage_flag;
		if (flags.test(vertex))   result |= vk::ShaderStageFlagBits::eVertex;
		if (flags.test(fragment)) result |= vk::ShaderStageFlagBits::eFragment;
		if (flags.test(compute))  result |= vk::ShaderStageFlagBits::eCompute;
		if (flags.test(task))     result |= vk::ShaderStageFlagBits::eTaskEXT;
		if (flags.test(mesh))     result |= vk::ShaderStageFlagBits::eMeshEXT;
		return result;
	}

	auto to_vk_descriptor_type(const gpu::descriptor_type type) -> vk::DescriptorType {
		switch (type) {
			case gpu::descriptor_type::uniform_buffer:          return vk::DescriptorType::eUniformBuffer;
			case gpu::descriptor_type::storage_buffer:          return vk::DescriptorType::eStorageBuffer;
			case gpu::descriptor_type::combined_image_sampler:  return vk::DescriptorType::eCombinedImageSampler;
			case gpu::descriptor_type::sampled_image:           return vk::DescriptorType::eSampledImage;
			case gpu::descriptor_type::storage_image:           return vk::DescriptorType::eStorageImage;
			case gpu::descriptor_type::sampler:                 return vk::DescriptorType::eSampler;
			case gpu::descriptor_type::acceleration_structure:  return vk::DescriptorType::eAccelerationStructureKHR;
		}
		return vk::DescriptorType::eStorageBuffer;
	}

	auto to_vk_format(const gpu::vertex_format fmt) -> vk::Format {
		switch (fmt) {
			case gpu::vertex_format::r32_sfloat:            return vk::Format::eR32Sfloat;
			case gpu::vertex_format::r32g32_sfloat:         return vk::Format::eR32G32Sfloat;
			case gpu::vertex_format::r32g32b32_sfloat:      return vk::Format::eR32G32B32Sfloat;
			case gpu::vertex_format::r32g32b32a32_sfloat:   return vk::Format::eR32G32B32A32Sfloat;
			case gpu::vertex_format::r32_sint:              return vk::Format::eR32Sint;
			case gpu::vertex_format::r32g32_sint:           return vk::Format::eR32G32Sint;
			case gpu::vertex_format::r32g32b32_sint:        return vk::Format::eR32G32B32Sint;
			case gpu::vertex_format::r32g32b32a32_sint:     return vk::Format::eR32G32B32A32Sint;
			case gpu::vertex_format::r32_uint:              return vk::Format::eR32Uint;
			case gpu::vertex_format::r32g32_uint:           return vk::Format::eR32G32Uint;
			case gpu::vertex_format::r32g32b32_uint:        return vk::Format::eR32G32B32Uint;
			case gpu::vertex_format::r32g32b32a32_uint:     return vk::Format::eR32G32B32A32Uint;
		}
		return vk::Format::eUndefined;
	}
}

export namespace gse::vulkan {
	struct shader_cache_entry {
		std::unordered_map<gpu::shader_stage, vk::raii::ShaderModule> modules;
		std::vector<vk::raii::DescriptorSetLayout> owned_layouts;
		std::vector<vk::DescriptorSetLayout> layout_handles;
		const shader_layout* external_layout = nullptr;
	};
}

export namespace gse::gpu {
	class device final : public non_copyable {
	public:
		[[nodiscard]] static auto create(const window& win,
			save::state& save
		) -> std::unique_ptr<device>;

		~device() override;

		[[nodiscard]] auto logical_device(
			this auto&& self
		) -> decltype(auto);

		[[nodiscard]] auto physical_device(
		) -> const vk::raii::PhysicalDevice&;

		[[nodiscard]] auto allocator(
		) -> vulkan::allocator&;

		[[nodiscard]] auto allocator(
		) const -> const vulkan::allocator&;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto surface_format(
		) const -> vk::Format;

		[[nodiscard]] auto compute_queue_family(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_queue(
		) -> const vk::raii::Queue&;

		auto add_transient_work(
			auto&& commands
		) -> void;

		auto cleanup_finished_frame_resources(
			std::uint32_t frame_index
		) -> void;

		auto transition_image_layout(
			const image& img,
			image_layout target
		) -> void;

		auto wait_idle(
		) const -> void;

		auto report_device_lost(
			std::string_view operation
		) -> void;

		[[nodiscard]] auto instance_config(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto device_config(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto queue_config(
		) -> vulkan::queue_config&;

		[[nodiscard]] auto command_config(
		) -> vulkan::command_config&;

		[[nodiscard]] auto descriptor_buffer_props(
		) const -> const vulkan::descriptor_buffer_properties&;

		[[nodiscard]] auto ray_tracing_enabled(
		) const -> bool;

		[[nodiscard]] auto video_encode_enabled(
		) const -> bool;

		[[nodiscard]] auto current_transient_frame(
		) const -> std::uint32_t;

		auto set_current_transient_frame(
			std::uint32_t frame
		) -> void;

		auto shader_cache(
			const shader& s
		) -> const vulkan::shader_cache_entry&;

		auto load_shader_layouts(
		) -> void;
	private:
		device(
			vulkan::instance_config&& instance,
			vulkan::device_config&& dev_config,
			std::unique_ptr<vulkan::allocator>&& alloc,
			vulkan::queue_config&& queue,
			vulkan::command_config&& command,
			std::unique_ptr<vulkan::descriptor_heap>&& desc_heap,
			vulkan::descriptor_buffer_properties&& desc_buf_props,
			vk::Format surface_format,
			std::uint32_t max_frames,
			bool ray_tracing_enabled,
			bool video_encode_enabled
		);

		vulkan::instance_config m_instance;
		vulkan::device_config m_device_config;
		std::unique_ptr<vulkan::allocator> m_allocator;
		vulkan::queue_config m_queue;
		vulkan::command_config m_command;
		std::unique_ptr<vulkan::descriptor_heap> m_descriptor_heap;
		vulkan::descriptor_buffer_properties m_descriptor_buffer_props;
		vk::Format m_surface_format;
		std::atomic<bool> m_device_lost_reported = false;
		std::vector<std::vector<vulkan::transient_gpu_work>> m_transient_work_graveyard;
		std::uint32_t m_current_transient_frame = 0;
		std::unordered_map<id, vulkan::shader_cache_entry> m_shader_cache;
		std::unordered_map<std::string, std::unique_ptr<shader_layout>> m_shader_layouts;
		bool m_ray_tracing_enabled = false;
		bool m_video_encode_enabled = false;
	};

	auto transition_image_layout(device& dev, const image& img, image_layout target) -> void;
}

auto gse::gpu::device::create(
	const window& win,
	save::state& save
) -> std::unique_ptr<device> {
	auto instance_data = vulkan::create_instance(window::vulkan_instance_extensions());
	instance_data.surface = vk::raii::SurfaceKHR(instance_data.instance, win.create_vulkan_surface(*instance_data.instance));
	auto [dev_config, queue, mesh_shaders_enabled, ray_tracing_enabled, video_encode_enabled, desc_buf_props] = vulkan::create_device_and_queues(instance_data);
	auto alloc = std::make_unique<vulkan::allocator>(dev_config.device, dev_config.physical_device, save);
	auto command = vulkan::create_command_objects(dev_config, instance_data);

	auto desc_heap = desc_buf_props.supported
		? std::make_unique<vulkan::descriptor_heap>(dev_config.device, dev_config.physical_device, desc_buf_props)
		: nullptr;

	const auto surface_format = [&] {
		const auto formats = dev_config.physical_device.getSurfaceFormatsKHR(*instance_data.surface);
		for (const auto& [format, colorSpace] : formats) {
			if (format == vk::Format::eB8G8R8A8Srgb && colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				return format;
			}
		}
		return formats[0].format;
	}();

	return std::unique_ptr<device>(new device(
		std::move(instance_data),
		std::move(dev_config),
		std::move(alloc),
		std::move(queue),
		std::move(command),
		std::move(desc_heap),
		std::move(desc_buf_props),
		surface_format,
		vulkan::max_frames_in_flight,
		ray_tracing_enabled,
		video_encode_enabled
	));
}

gse::gpu::device::device(
	vulkan::instance_config&& instance,
	vulkan::device_config&& dev_config,
	std::unique_ptr<vulkan::allocator>&& alloc,
	vulkan::queue_config&& queue,
	vulkan::command_config&& command,
	std::unique_ptr<vulkan::descriptor_heap>&& desc_heap,
	vulkan::descriptor_buffer_properties&& desc_buf_props,
	const vk::Format surface_format,
	const std::uint32_t max_frames,
	const bool ray_tracing_enabled,
	const bool video_encode_enabled
)
	: m_instance(std::move(instance)),
	  m_device_config(std::move(dev_config)),
	  m_allocator(std::move(alloc)),
	  m_queue(std::move(queue)),
	  m_command(std::move(command)),
	  m_descriptor_heap(std::move(desc_heap)),
	  m_descriptor_buffer_props(std::move(desc_buf_props)),
	  m_surface_format(surface_format),
	  m_ray_tracing_enabled(ray_tracing_enabled),
	  m_video_encode_enabled(video_encode_enabled) {
	m_transient_work_graveyard.resize(max_frames);
}

gse::gpu::device::~device() {
	log::println(log::category::runtime, "Destroying Device");
}

auto gse::gpu::device::logical_device(this auto&& self) -> decltype(auto) {
	return (self.m_device_config.device);
}

auto gse::gpu::device::physical_device() -> const vk::raii::PhysicalDevice& {
	return m_device_config.physical_device;
}

auto gse::gpu::device::allocator() -> vulkan::allocator& {
	return *m_allocator;
}

auto gse::gpu::device::allocator() const -> const vulkan::allocator& {
	return *m_allocator;
}

auto gse::gpu::device::descriptor_heap(this auto& self) -> decltype(auto) {
	assert(self.m_descriptor_heap.get(),
		std::source_location::current(),
		"Descriptor heap is unavailable because descriptor buffers are unsupported");
	return (*self.m_descriptor_heap);
}

auto gse::gpu::device::surface_format() const -> vk::Format {
	return m_surface_format;
}

auto gse::gpu::device::compute_queue_family() const -> std::uint32_t {
	return m_queue.compute_family_index;
}

auto gse::gpu::device::compute_queue() -> const vk::raii::Queue& {
	return m_queue.compute;
}

auto gse::gpu::device::add_transient_work(auto&& commands) -> void {
	const vk::CommandBufferAllocateInfo alloc_info{
		.commandPool = *m_command.pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	auto buffers = m_device_config.device.allocateCommandBuffers(alloc_info);
	vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

	constexpr vk::CommandBufferBeginInfo begin_info{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};

	command_buffer.begin(begin_info);

	std::vector<vulkan::buffer_resource> transient_buffers;
	if constexpr (std::is_void_v<std::invoke_result_t<decltype(commands), vk::raii::CommandBuffer&>>) {
		std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
	} else {
		transient_buffers = std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
	}

	command_buffer.end();

	vk::raii::Fence fence = m_device_config.device.createFence({});

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

	m_transient_work_graveyard[m_current_transient_frame].push_back({
		.command_buffer = std::move(command_buffer),
		.fence = std::move(fence),
		.transient_buffers = std::move(transient_buffers)
	});
}

auto gse::gpu::device::cleanup_finished_frame_resources(const std::uint32_t frame_index) -> void {
	for (auto& work : m_transient_work_graveyard[frame_index]) {
		if (*work.fence) {
			try {
				(void)m_device_config.device.waitForFences(
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
	m_transient_work_graveyard[frame_index].clear();
}

auto gse::gpu::device::wait_idle() const -> void {
	m_device_config.device.waitIdle();
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) -> void {
	if (bool expected = false; !m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		return;
	}

	log::println(log::level::error, log::category::vulkan, "Vulkan device lost during {}", operation);

	if (!m_device_config.device_fault_enabled) {
		log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
		return;
	}

	vk::DeviceFaultCountsEXT counts{};
	if (const auto result = m_device_config.device.getFaultInfoEXT(&counts, nullptr); result != vk::Result::eSuccess) {
		log::println(log::level::warning, log::category::vulkan, "Failed to query device fault counts: {}", vk::to_string(result));
		return;
	}

	std::vector<vk::DeviceFaultAddressInfoEXT> address_infos(counts.addressInfoCount);
	std::vector<vk::DeviceFaultVendorInfoEXT> vendor_infos(counts.vendorInfoCount);
	std::vector<std::byte> vendor_binary(
		m_device_config.device_fault_vendor_binary_enabled ? counts.vendorBinarySize : 0
	);

	counts.addressInfoCount = static_cast<std::uint32_t>(address_infos.size());
	counts.vendorInfoCount = static_cast<std::uint32_t>(vendor_infos.size());
	counts.vendorBinarySize = vendor_binary.size();

	vk::DeviceFaultInfoEXT fault_info{
		.pAddressInfos = address_infos.empty() ? nullptr : address_infos.data(),
		.pVendorInfos = vendor_infos.empty() ? nullptr : vendor_infos.data(),
		.pVendorBinaryData = vendor_binary.empty() ? nullptr : vendor_binary.data()
	};

	if (const auto result = m_device_config.device.getFaultInfoEXT(&counts, &fault_info); result != vk::Result::eSuccess) {
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
		const auto& [addressType, reportedAddress, addressPrecision] = address_infos[i];
		log::println(
			log::level::error,
			log::category::vulkan,
			"Fault address {}: type={}, reported=0x{:x}, precision=0x{:x}",
			i,
			vk::to_string(addressType),
			reportedAddress,
			addressPrecision
		);
	}

	for (std::size_t i = 0; i < vendor_infos.size(); ++i) {
		const auto& [description, vendorFaultCode, vendorFaultData] = vendor_infos[i];
		const char* vendor_description = description.data();
		log::println(
			log::level::error,
			log::category::vulkan,
			"Vendor fault {}: '{}' code=0x{:x} data=0x{:x}",
			i,
			(vendor_description && vendor_description[0] != '\0') ? std::string_view(vendor_description) : std::string_view("(no description)"),
			vendorFaultCode,
			vendorFaultData
		);
	}

	if (!vendor_binary.empty()) {
		log::println(log::level::error, log::category::vulkan, "Device fault vendor binary size: {} bytes", vendor_binary.size());
	}
}

auto gse::gpu::device::instance_config(this auto& self) -> auto& {
	return self.m_instance;
}

auto gse::gpu::device::device_config(this auto& self) -> auto& {
	return self.m_device_config;
}

auto gse::gpu::device::queue_config() -> vulkan::queue_config& {
	return m_queue;
}

auto gse::gpu::device::command_config() -> vulkan::command_config& {
	return m_command;
}

auto gse::gpu::device::descriptor_buffer_props() const -> const vulkan::descriptor_buffer_properties& {
	return m_descriptor_buffer_props;
}

auto gse::gpu::device::ray_tracing_enabled() const -> bool {
	return m_ray_tracing_enabled;
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
	return m_video_encode_enabled;
}

auto gse::gpu::device::current_transient_frame() const -> std::uint32_t {
	return m_current_transient_frame;
}

auto gse::gpu::device::set_current_transient_frame(const std::uint32_t frame) -> void {
	m_current_transient_frame = frame;
}

auto gse::gpu::transition_image_layout(device& dev, const image& img, const image_layout target) -> void {
	const bool is_depth = img.format() == image_format::d32_sfloat;
	const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
	const auto target_vk = [&] -> vk::ImageLayout {
		switch (target) {
			case image_layout::general:          return vk::ImageLayout::eGeneral;
			case image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                             return vk::ImageLayout::eUndefined;
		}
	}();
	const auto img_handle = img.native().image;
	const auto dst_stage = is_depth
		? (vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
		: vk::PipelineStageFlagBits2::eAllCommands;
	const auto dst_access = is_depth
		? (vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead)
		: vk::AccessFlagBits2::eShaderRead;

	dev.add_transient_work([img_handle, target_vk, aspect, dst_stage, dst_access](const auto& cmd) {
		const vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = dst_stage,
			.dstAccessMask = dst_access,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = target_vk,
			.image = img_handle,
			.subresourceRange = {
				.aspectMask = aspect,
				.baseMipLevel = 0, .levelCount = 1,
				.baseArrayLayer = 0, .layerCount = 1
			}
		};
		const vk::DependencyInfo dep{ 
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};
		cmd.pipelineBarrier2(dep);
		return std::vector<vulkan::buffer_resource>{};
	});

	const_cast<image&>(img).set_layout(target);
}

auto gse::gpu::device::transition_image_layout(const image& img, const image_layout target) -> void {
	gpu::transition_image_layout(*this, img, target);
}

auto gse::gpu::device::load_shader_layouts() -> void {
	if (!m_shader_layouts.empty()) return;

	const auto layouts_dir = config::baked_resource_path / "Layouts";
	if (!std::filesystem::exists(layouts_dir)) return;

	for (const auto& entry : std::filesystem::directory_iterator(layouts_dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".glayout") continue;

		auto layout = std::make_unique<shader_layout>(entry.path());
		layout->load(m_device_config.device);

		const auto& name = layout->name();
		log::println(log::category::assets, "Layout loaded: {}", name);
		m_shader_layouts[name] = std::move(layout);
	}
}

auto gse::gpu::device::shader_cache(const shader& s) -> const vulkan::shader_cache_entry& {
	if (const auto it = m_shader_cache.find(s.id()); it != m_shader_cache.end()) {
		return it->second;
	}

	vulkan::shader_cache_entry entry;

	auto create_module = [&](const shader_stage stage) -> vk::raii::ShaderModule {
		const auto spirv = s.spirv(stage);
		if (spirv.empty()) return nullptr;

		return m_device_config.device.createShaderModule({
			.codeSize = spirv.size_bytes(),
			.pCode = spirv.data()
		});
	};

	if (s.is_compute()) {
		entry.modules.emplace(shader_stage::compute, create_module(shader_stage::compute));
	} else if (s.is_mesh_shader()) {
		entry.modules.emplace(shader_stage::task, create_module(shader_stage::task));
		entry.modules.emplace(shader_stage::mesh, create_module(shader_stage::mesh));
		entry.modules.emplace(shader_stage::fragment, create_module(shader_stage::fragment));
	} else {
		entry.modules.emplace(shader_stage::vertex, create_module(shader_stage::vertex));
		entry.modules.emplace(shader_stage::fragment, create_module(shader_stage::fragment));
	}

	const auto& layout_name = s.layout_name();
	const auto& [sets] = s.layout_data();

	const shader_layout* external_layout = nullptr;
	if (!layout_name.empty()) {
		if (const auto it = m_shader_layouts.find(layout_name); it != m_shader_layouts.end()) {
			external_layout = it->second.get();
		}
	}

	if (external_layout) {
		entry.layout_handles = external_layout->vk_layouts();
		entry.external_layout = external_layout;
		auto [result_it, inserted] = m_shader_cache.emplace(s.id(), std::move(entry));
		return result_it->second;
	}

	std::uint32_t max_set_index = 0;
	for (const auto& type : sets | std::views::keys) {
		max_set_index = std::max(max_set_index, static_cast<std::uint32_t>(type));
	}

	entry.owned_layouts.reserve(max_set_index + 1);
	while (entry.owned_layouts.size() <= max_set_index) {
		entry.owned_layouts.emplace_back(nullptr);
	}

	for (const auto& [type, set_data] : sets) {
		const auto set_idx = static_cast<std::uint32_t>(type);

		std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
		raw_bindings.reserve(set_data.bindings.size());

		for (const auto& b : set_data.bindings) {
			raw_bindings.push_back({
				.binding = b.desc.binding,
				.descriptorType = vulkan::to_vk_descriptor_type(b.desc.type),
				.descriptorCount = b.desc.count,
				.stageFlags = vulkan::to_vk_stage_flags(b.desc.stages),
				.pImmutableSamplers = nullptr
			});
		}

		vk::DescriptorSetLayoutCreateInfo ci{
			.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
			.bindingCount = static_cast<std::uint32_t>(raw_bindings.size()),
			.pBindings = raw_bindings.data()
		};

		entry.owned_layouts[set_idx] = m_device_config.device.createDescriptorSetLayout(ci);
	}

	for (std::uint32_t i = 0; i <= max_set_index; ++i) {
		if (!*entry.owned_layouts[i]) {
			vk::DescriptorSetLayoutCreateInfo ci{
				.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
				.bindingCount = 0,
				.pBindings = nullptr
			};
			entry.owned_layouts[i] = m_device_config.device.createDescriptorSetLayout(ci);
		}
	}

	entry.layout_handles.reserve(entry.owned_layouts.size());
	for (const auto& l : entry.owned_layouts) {
		entry.layout_handles.push_back(*l);
	}

	auto [result_it, inserted] = m_shader_cache.emplace(s.id(), std::move(entry));
	return result_it->second;
}
