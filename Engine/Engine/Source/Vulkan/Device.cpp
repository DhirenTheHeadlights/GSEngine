module gse.vulkan;

import std;
import vulkan;

import gse.assert;
import gse.core;
import gse.log;

namespace gse::vulkan {
	template <typename Feat>
	struct optional_feature {
		Feat features{};
		std::string_view extension_name;
		std::string_view log_yes;
		std::string_view log_no;
		bool supported = false;

		auto attach(void* prev) -> void* {
			features.pNext = prev;
			return supported ? static_cast<void*>(&features) : prev;
		}

		auto register_ext(std::vector<const char*>& exts) const -> void {
			if (supported && !extension_name.empty()) {
				exts.push_back(extension_name.data());
			}
		}

		auto log() const -> void {
			if (supported) {
				if (!log_yes.empty()) {
					log::println(log::category::vulkan, "{}", log_yes);
				}
			}
			else if (!log_no.empty()) {
				log::println(log::level::info, log::category::vulkan, "{}", log_no);
			}
		}
	};
}

auto gse::vulkan::host_transition_image_to_general(const device& dev, const gpu::handle<image> img, const gpu::image_aspect_flag aspect, const std::uint32_t layer_count) -> void {
	const vk::ImageAspectFlags aspect_mask = aspect == gpu::image_aspect_flag::depth
		? vk::ImageAspectFlagBits::eDepth
		: vk::ImageAspectFlagBits::eColor;
	const vk::HostImageLayoutTransitionInfoEXT transition{
		.image = std::bit_cast<vk::Image>(img),
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eGeneral,
		.subresourceRange = {
			.aspectMask = aspect_mask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = layer_count,
		},
	};
	dev.raii_device().transitionImageLayoutEXT(transition);
}

auto gse::vulkan::host_upload_image_layers(const device& dev, const gpu::handle<image> img, const std::span<const void* const> layer_pointers, const vec2u extent) -> void {
	const auto layer_count = static_cast<std::uint32_t>(layer_pointers.size());
	host_transition_image_to_general(dev, img, gpu::image_aspect_flag::color, layer_count);

	std::vector<vk::MemoryToImageCopyEXT> regions;
	regions.reserve(layer_count);
	for (std::uint32_t i = 0; i < layer_count; ++i) {
		regions.push_back({
			.pHostPointer = layer_pointers[i],
			.memoryRowLength = 0,
			.memoryImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = i,
				.layerCount = 1,
			},
			.imageOffset = { 0, 0, 0 },
			.imageExtent = { extent.x(), extent.y(), 1 },
		});
	}

	const vk::CopyMemoryToImageInfoEXT info{
		.flags = {},
		.dstImage = std::bit_cast<vk::Image>(img),
		.dstImageLayout = vk::ImageLayout::eGeneral,
		.regionCount = static_cast<std::uint32_t>(regions.size()),
		.pRegions = regions.data(),
	};
	dev.raii_device().copyMemoryToImageEXT(info);
}

auto gse::vulkan::wait_for_fence(const device& dev, const gpu::handle<fence> fence, const std::uint64_t timeout_ns) -> gpu::result {
	const auto vk_fence = std::bit_cast<vk::Fence>(fence);
	const auto vk_result = dev.raii_device().waitForFences(vk_fence, vk::True, timeout_ns);
	return from_vk(vk_result);
}

auto gse::vulkan::reset_fence(const device& dev, const gpu::handle<fence> fence) -> void {
	dev.raii_device().resetFences(std::bit_cast<vk::Fence>(fence));
}

auto gse::vulkan::acquire_next_image(const device& dev, const gpu::handle<swap_chain> swapchain, const gpu::handle<semaphore> wait_semaphore, const std::uint64_t timeout_ns) -> gpu::acquire_next_image_result {
	const vk::AcquireNextImageInfoKHR info{
		.swapchain = std::bit_cast<vk::SwapchainKHR>(swapchain),
		.timeout = timeout_ns,
		.semaphore = std::bit_cast<vk::Semaphore>(wait_semaphore),
		.fence = nullptr,
		.deviceMask = 1,
	};
	auto [vk_result, image_index] = dev.raii_device().acquireNextImage2KHR(info);
	return {
		.result = from_vk(vk_result),
		.image_index = image_index,
	};
}

gse::vulkan::device::~device() {
	clean_up();
}

gse::vulkan::device::device(device&& other) noexcept
	: m_physical_device(std::move(other.m_physical_device)), m_device(std::move(other.m_device)), m_fault_enabled(other.m_fault_enabled), m_vendor_binary_fault_enabled(other.m_vendor_binary_fault_enabled), m_queue_families(other.m_queue_families), m_pools(std::move(other.m_pools)), m_live_allocation_count(other.m_live_allocation_count.load()), m_next_allocation_id(other.m_next_allocation_id.load()), m_cleaned_up(other.m_cleaned_up), m_settings(other.m_settings), m_live_allocations(std::move(other.m_live_allocations)) {
}

auto gse::vulkan::device::operator=(device&& other) noexcept -> device& {
	if (this != &other) {
		clean_up();
		m_physical_device = std::move(other.m_physical_device);
		m_device = std::move(other.m_device);
		m_fault_enabled = other.m_fault_enabled;
		m_vendor_binary_fault_enabled = other.m_vendor_binary_fault_enabled;
		m_queue_families = other.m_queue_families;
		m_pools = std::move(other.m_pools);
		m_live_allocation_count = other.m_live_allocation_count.load();
		m_next_allocation_id = other.m_next_allocation_id.load();
		m_cleaned_up = other.m_cleaned_up;
		m_settings = other.m_settings;
		m_live_allocations = std::move(other.m_live_allocations);
	}
	return *this;
}

auto gse::vulkan::device::queue_family(const gpu::queue_type queue) const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(queue)];
}

auto gse::vulkan::device::graphics_family() const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)];
}

auto gse::vulkan::device::compute_family() const -> std::uint32_t {
	return m_queue_families[static_cast<std::size_t>(gpu::queue_type::compute)];
}

auto gse::vulkan::device::families_distinct() const -> bool {
	for (std::size_t i = 1; i < gpu::queue_type_count; ++i) {
		if (m_queue_families[i] != m_queue_families[0]) {
			return true;
		}
	}
	return false;
}

auto gse::vulkan::device::create(const instance& instance_data, device::settings& cfg, aftermath& aftermath_tracker) -> device_creation_result {
	const auto devices = instance_data.enumerate_physical_devices();
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

	const auto physical_device_properties = physical_device.getProperties();
	log::println(
		log::category::vulkan,
		"Selected GPU: {}",
		std::string_view(physical_device_properties.deviceName.data())
	);

	const auto families = find_queue_families(physical_device, instance_data.raii_surface());

	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	std::set unique_queue_families = {
		families.graphics_family.value(),
		families.present_family.value(),
		families.compute_family.value(),
	};

	if (families.video_encode_family.has_value()) {
		unique_queue_families.insert(families.video_encode_family.value());
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
		return std::ranges::any_of(
			available_extensions,
			[&](const auto& extension) {
				return std::strcmp(extension.extensionName.data(), extension_name) == 0;
			}
		);
	};

	assert(supports_extension(vk::EXTShaderObjectExtensionName), "VK_EXT_shader_object is required");
	assert(supports_extension(vk::EXTExtendedDynamicState3ExtensionName), "VK_EXT_extended_dynamic_state3 is required");
	assert(
		supports_extension(vk::EXTVertexInputDynamicStateExtensionName),
		"VK_EXT_vertex_input_dynamic_state is required"
	);
	assert(supports_extension(vk::EXTDescriptorHeapExtensionName), "VK_EXT_descriptor_heap is required");
	assert(
		supports_extension(vk::KHRDeferredHostOperationsExtensionName) && supports_extension(vk::KHRAccelerationStructureExtensionName) && supports_extension(vk::KHRRayQueryExtensionName),
		"Ray tracing extensions are required"
	);

	const bool video_encode_extensions_available =
		families.video_encode_family.has_value() &&
		supports_extension(vk::KHRVideoQueueExtensionName) &&
		supports_extension(vk::KHRVideoEncodeQueueExtensionName);

	const auto feature_chain = physical_device.getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceMeshShaderFeaturesEXT,
		vk::PhysicalDeviceFaultFeaturesEXT,
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
		vk::PhysicalDeviceRayQueryFeaturesKHR,
		vk::PhysicalDeviceRobustness2FeaturesEXT,
		vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR,
		vk::PhysicalDeviceHostImageCopyFeaturesEXT,
		vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT,
		vk::PhysicalDevicePresentIdFeaturesKHR,
		vk::PhysicalDevicePresentWaitFeaturesKHR,
		vk::PhysicalDeviceDescriptorHeapFeaturesEXT,
		vk::PhysicalDeviceShaderObjectFeaturesEXT,
		vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT,
		vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT
	>();

	const auto& vk12_query = feature_chain.get<vk::PhysicalDeviceVulkan12Features>();
	const auto& mesh_shader_query = feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
	const auto& as_query = feature_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
	const auto& rq_query = feature_chain.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
	const auto& fault_query = feature_chain.get<vk::PhysicalDeviceFaultFeaturesEXT>();
	const auto& robustness2_query = feature_chain.get<vk::PhysicalDeviceRobustness2FeaturesEXT>();
	const auto& unified_layouts_query = feature_chain.get<vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>();
	const auto& host_image_copy_query = feature_chain.get<vk::PhysicalDeviceHostImageCopyFeaturesEXT>();
	const auto& swapchain_maintenance1_query = feature_chain.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>();
	const auto& present_id_query = feature_chain.get<vk::PhysicalDevicePresentIdFeaturesKHR>();
	const auto& present_wait_query = feature_chain.get<vk::PhysicalDevicePresentWaitFeaturesKHR>();
	const auto& descriptor_heap_query = feature_chain.get<vk::PhysicalDeviceDescriptorHeapFeaturesEXT>();
	const auto& shader_object_query = feature_chain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>();
	const auto& eds3_query = feature_chain.get<vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT>();
	const auto& vertex_input_dynamic_state_query = feature_chain.get<vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT>();

	assert(mesh_shader_query.meshShader && mesh_shader_query.taskShader, "Mesh shaders required");
	assert(as_query.accelerationStructure && rq_query.rayQuery, "Ray tracing features required");
	assert(descriptor_heap_query.descriptorHeap, "VK_EXT_descriptor_heap feature required");
	assert(shader_object_query.shaderObject, "VK_EXT_shader_object feature required");
	assert(vertex_input_dynamic_state_query.vertexInputDynamicState, "vertexInputDynamicState required");

	const bool device_fault_supported = supports_extension(vk::EXTDeviceFaultExtensionName) && fault_query.deviceFault;
	const bool device_fault_vendor_binary_supported = device_fault_supported && fault_query.deviceFaultVendorBinary;
	const bool av1_encode_supported = video_encode_extensions_available && supports_extension(vk::KHRVideoEncodeAv1ExtensionName);

	assert(
		supports_extension(vk::EXTRobustness2ExtensionName) && robustness2_query.robustBufferAccess2,
		"VK_EXT_robustness2 is required"
	);
	assert(
		supports_extension(vk::KHRUnifiedImageLayoutsExtensionName) && unified_layouts_query.unifiedImageLayouts,
		"VK_KHR_unified_image_layouts is required"
	);
	assert(
		supports_extension(vk::EXTHostImageCopyExtensionName) && host_image_copy_query.hostImageCopy,
		"VK_EXT_host_image_copy is required"
	);
	assert(
		supports_extension(vk::EXTSwapchainMaintenance1ExtensionName) && swapchain_maintenance1_query.swapchainMaintenance1,
		"VK_EXT_swapchain_maintenance1 is required"
	);
	assert(
		supports_extension(vk::KHRPresentIdExtensionName) && present_id_query.presentId,
		"VK_KHR_present_id is required"
	);
	assert(
		supports_extension(vk::KHRPresentWaitExtensionName) && present_wait_query.presentWait,
		"VK_KHR_present_wait is required"
	);

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
		.subgroupSizeControl = vk::True,
		.computeFullSubgroups = vk::True,
		.synchronization2 = vk::True,
		.dynamicRendering = vk::True,
	};
	vk::PhysicalDeviceVulkan12Features vulkan12_features{
		.pNext = &vulkan13_features,
		.storageBuffer8BitAccess = vk::True,
		.uniformAndStorageBuffer8BitAccess = vk::True,
		.shaderInt8 = vk::True,
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
		.bufferDeviceAddressCaptureReplay = vk12_query.bufferDeviceAddressCaptureReplay,
	};
	vk::PhysicalDeviceVulkan11Features vulkan11_features{
		.pNext = &vulkan12_features,
		.storageBuffer16BitAccess = vk::True,
		.uniformAndStorageBuffer16BitAccess = vk::True,
		.shaderDrawParameters = vk::True,
	};
	vk::PhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
		.pNext = &vulkan11_features,
		.taskShader = vk::True,
		.meshShader = vk::True,
	};
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features{
		.pNext = &mesh_shader_features,
		.accelerationStructure = vk::True,
		.accelerationStructureCaptureReplay = as_query.accelerationStructureCaptureReplay,
	};
	vk::PhysicalDeviceRayQueryFeaturesKHR ray_query_features{
		.pNext = &as_features,
		.rayQuery = vk::True,
	};
	vk::PhysicalDeviceDescriptorHeapFeaturesEXT descriptor_heap_features{
		.pNext = &ray_query_features,
		.descriptorHeap = vk::True,
	};
	vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_features{
		.pNext = &descriptor_heap_features,
		.shaderObject = vk::True,
	};
	auto eds3_features = eds3_query;
	eds3_features.pNext = &shader_object_features;
	vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{
		.pNext = &eds3_features,
		.vertexInputDynamicState = vk::True,
	};
	vk::PhysicalDeviceRobustness2FeaturesEXT robustness2_features{
		.pNext = &vertex_input_dynamic_state_features,
		.robustBufferAccess2 = vk::True,
		.robustImageAccess2 = vk::True,
		.nullDescriptor = vk::True,
	};
	vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR unified_layouts_features{
		.pNext = &robustness2_features,
		.unifiedImageLayouts = vk::True,
	};
	vk::PhysicalDeviceHostImageCopyFeaturesEXT host_image_copy_features{
		.pNext = &unified_layouts_features,
		.hostImageCopy = vk::True,
	};
	vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1_features{
		.pNext = &host_image_copy_features,
		.swapchainMaintenance1 = vk::True,
	};
	vk::PhysicalDevicePresentIdFeaturesKHR present_id_features{
		.pNext = &swapchain_maintenance1_features,
		.presentId = vk::True,
	};
	vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{
		.pNext = &present_id_features,
		.presentWait = vk::True,
	};

	optional_feature<vk::PhysicalDeviceFaultFeaturesEXT> fault{
		.features = {
			.deviceFault = device_fault_supported,
			.deviceFaultVendorBinary = device_fault_vendor_binary_supported,
		},
		.extension_name = vk::EXTDeviceFaultExtensionName,
		.log_yes = "Device fault support detected",
		.supported = device_fault_supported,
	};
	optional_feature<vk::PhysicalDeviceVideoEncodeAV1FeaturesKHR> av1_encode{
		.features = {
			.videoEncodeAV1 = vk::True
		},
		.supported = av1_encode_supported,
	};

	std::vector device_extensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRDynamicRenderingExtensionName,
		vk::KHRMaintenance5ExtensionName,
		vk::KHRMaintenance6ExtensionName,
		vk::EXTCalibratedTimestampsExtensionName,
		vk::EXTPageableDeviceLocalMemoryExtensionName,
		vk::EXTMemoryPriorityExtensionName,
		vk::EXTMeshShaderExtensionName,
		vk::KHRDeferredHostOperationsExtensionName,
		vk::KHRAccelerationStructureExtensionName,
		vk::KHRRayQueryExtensionName,
		vk::EXTShaderObjectExtensionName,
		vk::EXTExtendedDynamicState3ExtensionName,
		vk::EXTVertexInputDynamicStateExtensionName,
		vk::EXTDescriptorHeapExtensionName,
		vk::EXTRobustness2ExtensionName,
		vk::KHRUnifiedImageLayoutsExtensionName,
		vk::EXTHostImageCopyExtensionName,
		vk::EXTSwapchainMaintenance1ExtensionName,
		vk::KHRPresentIdExtensionName,
		vk::KHRPresentWaitExtensionName,
	};

	void* chain_head = &present_wait_features;
	auto process = [&](auto&... features) {
		((chain_head = features.attach(chain_head)), ...);
		(features.register_ext(device_extensions), ...);
		(features.log(), ...);
	};
	process(fault, av1_encode);

	for (const auto* aftermath_ext : aftermath_tracker.required_device_extensions()) {
		if (supports_extension(aftermath_ext)) {
			device_extensions.push_back(aftermath_ext);
		}
	}

	if (video_encode_extensions_available) {
		device_extensions.push_back(vk::KHRVideoQueueExtensionName);
		device_extensions.push_back(vk::KHRVideoEncodeQueueExtensionName);
		if (av1_encode_supported) {
			device_extensions.push_back(vk::KHRVideoEncodeAv1ExtensionName);
		}
		if (supports_extension(vk::KHRVideoEncodeH265ExtensionName)) {
			device_extensions.push_back(vk::KHRVideoEncodeH265ExtensionName);
		}
	}

	chain_head = aftermath_tracker.device_create_info_pnext(chain_head);

	vk::PhysicalDeviceFeatures2 features2{
		.pNext = chain_head,
		.features = {
			.robustBufferAccess = vk::True,
			.drawIndirectFirstInstance = vk::True,
			.fillModeNonSolid = vk::True,
			.samplerAnisotropy = vk::True,
			.pipelineStatisticsQuery = vk::True,
			.shaderInt64 = vk::True,
		},
	};

	vk::DeviceCreateInfo create_info{
		.pNext = &features2,
		.flags = {},
		.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
		.pQueueCreateInfos = queue_create_infos.data(),
		.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
		.ppEnabledExtensionNames = device_extensions.data(),
	};

	vk::raii::Device logical_device = physical_device.createDevice(create_info);

	vk::detail::defaultDispatchLoaderDynamic.init(*logical_device);

	log::println(log::category::vulkan, "Logical Device Created Successfully!");

	vk::raii::Queue graphics_queue = logical_device.getQueue(families.graphics_family.value(), 0);
	vk::raii::Queue present_queue = logical_device.getQueue(families.present_family.value(), 0);
	vk::raii::Queue compute_queue = logical_device.getQueue(families.compute_family.value(), 0);

	auto result = device_creation_result{
		.device = device(
			std::move(physical_device),
			std::move(logical_device),
			cfg,
			device_fault_supported,
			device_fault_vendor_binary_supported,
			families.graphics_family.value(),
			families.compute_family.value()
		),
		.queue = queue(
			std::move(graphics_queue),
			std::move(present_queue),
			std::move(compute_queue),
			families.graphics_family.value(),
			families.compute_family.value()
		),
		.families = families,
		.video_encode_enabled = video_encode_extensions_available,
	};

	if (video_encode_extensions_available) {
		result.queue.set_video_encode(
			result.device.raii_device().getQueue(families.video_encode_family.value(), 0),
			families.video_encode_family.value()
		);
		log::println(
			log::category::vulkan,
			"Video encode queue acquired (family {})",
			families.video_encode_family.value()
		);
	}

	return result;
}

auto gse::vulkan::device::device_handle() const -> gpu::handle<device> {
	return std::bit_cast<gpu::handle<device>>(*m_device);
}

auto gse::vulkan::device::fault_enabled() const -> bool {
	return m_fault_enabled;
}

auto gse::vulkan::device::vendor_binary_fault_enabled() const -> bool {
	return m_vendor_binary_fault_enabled;
}

auto gse::vulkan::device::wait_idle() const -> void {
	m_device.waitIdle();
}

auto gse::vulkan::device::timestamp_period() const -> float {
	return m_physical_device.getProperties().limits.timestampPeriod;
}

auto gse::vulkan::device::query_fault_counts(gpu::device_fault_counts& counts) const -> gpu::result {
	vk::DeviceFaultCountsEXT vk_counts{};
	const auto vk_result = m_device.getFaultInfoEXT(&vk_counts, nullptr);
	counts.address_info_count = vk_counts.addressInfoCount;
	counts.vendor_info_count = vk_counts.vendorInfoCount;
	counts.vendor_binary_size = vk_counts.vendorBinarySize;
	return from_vk(vk_result);
}

auto gse::vulkan::device::query_fault_info(gpu::device_fault_counts& counts, gpu::device_fault_info& info) const -> gpu::result {
	std::vector<vk::DeviceFaultAddressInfoEXT> vk_address_infos(counts.address_info_count);
	std::vector<vk::DeviceFaultVendorInfoEXT> vk_vendor_infos(counts.vendor_info_count);
	std::vector<std::byte> vk_vendor_binary(counts.vendor_binary_size);

	vk::DeviceFaultCountsEXT vk_counts{
		.addressInfoCount = counts.address_info_count,
		.vendorInfoCount = counts.vendor_info_count,
		.vendorBinarySize = counts.vendor_binary_size,
	};
	vk::DeviceFaultInfoEXT vk_info{
		.pAddressInfos = vk_address_infos.empty() ? nullptr : vk_address_infos.data(),
		.pVendorInfos = vk_vendor_infos.empty() ? nullptr : vk_vendor_infos.data(),
		.pVendorBinaryData = vk_vendor_binary.empty() ? nullptr : vk_vendor_binary.data(),
	};

	const auto vk_result = m_device.getFaultInfoEXT(&vk_counts, &vk_info);

	counts.address_info_count = vk_counts.addressInfoCount;
	counts.vendor_info_count = vk_counts.vendorInfoCount;
	counts.vendor_binary_size = vk_counts.vendorBinarySize;

	info.description = vk_info.description.data();

	static thread_local std::vector<gpu::device_fault_address_info> tls_addresses;
	tls_addresses.clear();
	tls_addresses.reserve(vk_address_infos.size());
	for (const auto& a : vk_address_infos) {
		tls_addresses.push_back({
			.address_type = static_cast<std::uint32_t>(a.addressType),
			.reported_address = a.reportedAddress,
			.address_precision = a.addressPrecision,
		});
	}
	info.address_infos = tls_addresses;

	static thread_local std::vector<gpu::device_fault_vendor_info> tls_vendors;
	tls_vendors.clear();
	tls_vendors.reserve(vk_vendor_infos.size());
	for (const auto& v : vk_vendor_infos) {
		tls_vendors.push_back({
			.description = v.description.data(),
			.vendor_fault_code = v.vendorFaultCode,
			.vendor_fault_data = v.vendorFaultData,
		});
	}
	info.vendor_infos = tls_vendors;

	static thread_local std::vector<std::byte> tls_binary;
	tls_binary = std::move(vk_vendor_binary);
	info.vendor_binary = tls_binary;

	return from_vk(vk_result);
}

auto gse::vulkan::device::create_buffer(const vk::BufferCreateInfo& buffer_info, const void* data, const std::string_view tag, const std::source_location& loc) -> basic_buffer<device> {
	auto actual_buffer_info = buffer_info;
	constexpr auto device_addressable_usage = vk::BufferUsageFlagBits::eUniformBuffer |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress;
	const bool needs_device_address = static_cast<bool>(actual_buffer_info.usage & device_addressable_usage);

	if (needs_device_address) {
		actual_buffer_info.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}

	const auto shared_family_indices = m_queue_families;
	if (families_distinct() && actual_buffer_info.sharingMode == vk::SharingMode::eExclusive) {
		actual_buffer_info.sharingMode = vk::SharingMode::eConcurrent;
		actual_buffer_info.queueFamilyIndexCount = static_cast<std::uint32_t>(shared_family_indices.size());
		actual_buffer_info.pQueueFamilyIndices = shared_family_indices.data();
	}

	auto buffer = (*m_device).createBuffer(actual_buffer_info, nullptr);
	const auto requirements = (*m_device).getBufferMemoryRequirements(buffer);

	basic_allocation<device> alloc{};
	bool success = false;

	if (data) {
		constexpr vk::MemoryPropertyFlags required_flags =
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		if (auto expected_alloc = allocate(requirements, required_flags, tag, loc, needs_device_address)) {
			alloc = std::move(*expected_alloc);
			success = true;
		}
	}
	else {
		for (const auto property_preferences = memory_flag_preferences(actual_buffer_info.usage); const auto& props : property_preferences) {
			if (auto expected_alloc = allocate(requirements, props, tag, loc, needs_device_address)) {
				alloc = std::move(*expected_alloc);
				success = true;
				break;
			}
		}
	}

	assert(success, "Failed to allocate memory for buffer after trying all preferences.");

	(*m_device).bindBufferMemory(buffer, std::bit_cast<vk::DeviceMemory>(alloc.memory()), alloc.offset());

	if ((m_settings && m_settings->name_resources)) {
		const auto& debug_info = alloc.debug_info();
		const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
		const auto name = debug_info.tag.empty()
			? std::format("Buffer ({}:{})", file,
						  debug_info.creation_location.line())
			: std::format(
				  "Buffer '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );

		(*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eBuffer,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(buffer)),
			.pObjectName = name.c_str(),
		});
	}

	if (data && alloc.mapped()) {
		gse::memcpy(alloc.mapped(), data, actual_buffer_info.size);
	}
	else if (data && !alloc.mapped()) {
		assert(false, "Buffer created with data, but the allocated memory is not mappable.");
	}

	return basic_buffer<device>(
		std::bit_cast<gpu::handle<vulkan::buffer>>(buffer),
		std::move(alloc),
		actual_buffer_info.size
	);
}

auto gse::vulkan::device::create_buffer(const gpu::buffer_create_info& buffer_info, const void* data, const std::string_view tag, const std::source_location& loc) -> basic_buffer<device> {
	const vk::BufferCreateInfo vk_info{
		.pNext = buffer_info.pnext,
		.size = buffer_info.size,
		.usage = to_vk(buffer_info.usage),
	};
	return create_buffer(vk_info, data, tag, loc);
}

auto gse::vulkan::device::create_image(const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, const vk::ImageViewCreateInfo& view_info, const void* data, const std::string_view tag, const std::source_location loc, gpu::image_view_create_info engine_view_info) -> basic_image<device> {
	auto image = (*m_device).createImage(info, nullptr);
	auto image_guard = make_scope_exit([this, image] {
		(*m_device).destroyImage(image, nullptr);
	});

	const auto requirements = (*m_device).getImageMemoryRequirements(image);

	auto expected_alloc = allocate(requirements, properties, tag, loc);

	if (!expected_alloc) {
		assert(
			false,
			"Failed to allocate memory for image with size {} and usage {} after trying all preferences.",
			requirements.size,
			to_string(info.usage)
		);
	}

	basic_allocation<device> alloc = std::move(*expected_alloc);

	(*m_device).bindImageMemory(image, std::bit_cast<vk::DeviceMemory>(alloc.memory()), alloc.offset());

	if (data && alloc.mapped()) {
		gse::memcpy(alloc.mapped(), data, requirements.size);
	}

	vk::ImageView view;

	if (view_info != vk::ImageViewCreateInfo{}) {
		assert(!view_info.image, "Image view info must not have an image set yet!");
		vk::ImageViewCreateInfo actual_view_info = view_info;
		actual_view_info.image = image;
		view = (*m_device).createImageView(actual_view_info, nullptr);
	}
	else {
		view = (*m_device).createImageView(
			vk::ImageViewCreateInfo{
				.image = image,
				.viewType = vk::ImageViewType::e2D,
				.format = info.format,
				.subresourceRange = {
					.aspectMask = info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = info.mipLevels,
					.baseArrayLayer = 0,
					.layerCount = info.arrayLayers,
				},
			},
			nullptr
		);
	}

	image_guard.release();

	if (m_settings && m_settings->name_resources) {
		const auto& debug_info = alloc.debug_info();
		const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
		const auto image_name = debug_info.tag.empty()
			? std::format("Image ({}:{})", file, debug_info.creation_location.line())
			: std::format(
				  "Image '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );
		const auto view_name = debug_info.tag.empty()
			? std::format(
				  "ImageView ({}:{})",
				  file,
				  debug_info.creation_location.line()
			  )
			: std::format(
				  "ImageView '{}' ({}:{})",
				  debug_info.tag,
				  file,
				  debug_info.creation_location.line()
			  );

		(*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eImage,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(image)),
			.pObjectName = image_name.c_str(),
		});
		(*m_device).setDebugUtilsObjectNameEXT({
			.objectType = vk::ObjectType::eImageView,
			.objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(view)),
			.pObjectName = view_name.c_str(),
		});
	}

	return basic_image<device>(
		std::bit_cast<gpu::handle<vulkan::image>>(image),
		std::bit_cast<gpu::handle<vulkan::image_view>>(view),
		static_cast<gpu::image_format_value>(info.format),
		vec3u{ info.extent.width, info.extent.height, info.extent.depth },
		engine_view_info,
		std::move(alloc)
	);
}

auto gse::vulkan::device::create_image(const gpu::image_create_info& info, const gpu::memory_property_flags properties, const gpu::image_view_create_info& view_info, const void* data, const std::string_view tag, const std::source_location loc) -> basic_image<device> {
	const vk::ImageCreateInfo vk_info{
		.flags = to_vk(info.flags),
		.imageType = to_vk(info.type),
		.format = to_vk(info.format),
		.extent = vk::Extent3D{ info.extent.x(), info.extent.y(), info.extent.z() },
		.mipLevels = info.mip_levels,
		.arrayLayers = info.array_layers,
		.samples = to_vk(info.samples),
		.tiling = vk::ImageTiling::eOptimal,
		.usage = to_vk(info.usage),
	};
	auto aspects = view_info.aspects;
	if (!aspects) {
		aspects = info.usage.test(gpu::image_flag::depth_attachment)
			? gpu::image_aspect_flags{ gpu::image_aspect_flag::depth }
			: gpu::image_aspect_flags{ gpu::image_aspect_flag::color };
	}
	const vk::ImageViewCreateInfo vk_view_info{
		.viewType = to_vk(view_info.view_type),
		.format = to_vk(view_info.format == gpu::image_format::r8g8b8a8_unorm ? info.format : view_info.format),
		.subresourceRange = {
			.aspectMask = to_vk(aspects),
			.baseMipLevel = view_info.base_mip_level,
			.levelCount = view_info.level_count == 1 ? info.mip_levels : view_info.level_count,
			.baseArrayLayer = view_info.base_array_layer,
			.layerCount = view_info.layer_count == 1 ? info.array_layers : view_info.layer_count,
		},
	};
	auto resolved_view_info = view_info;
	resolved_view_info.aspects = aspects;
	if (view_info.level_count == 1) {
		resolved_view_info.level_count = info.mip_levels;
	}
	if (view_info.layer_count == 1) {
		resolved_view_info.layer_count = info.array_layers;
	}
	if (view_info.format == gpu::image_format::r8g8b8a8_unorm) {
		resolved_view_info.format = info.format;
	}
	return create_image(vk_info, to_vk(properties), vk_view_info, data, tag, loc, resolved_view_info);
}

auto gse::vulkan::device::live_allocation_count() const -> std::uint32_t {
	return m_live_allocation_count.load();
}

auto gse::vulkan::device::tracking_enabled() const -> bool {
	return (m_settings && m_settings->tracking_enabled);
}

auto gse::vulkan::device::destroy_buffer(const gpu::handle<buffer> buffer) const -> void {
	(*m_device).destroyBuffer(std::bit_cast<vk::Buffer>(buffer), nullptr);
}

auto gse::vulkan::device::buffer_device_address(const gpu::handle<buffer> buffer) const -> gpu::device_address {
	return (*m_device).getBufferAddress(vk::BufferDeviceAddressInfo{
		.buffer = std::bit_cast<vk::Buffer>(buffer),
	});
}

auto gse::vulkan::device::destroy_image(const gpu::handle<image> image) const -> void {
	(*m_device).destroyImage(std::bit_cast<vk::Image>(image), nullptr);
}

auto gse::vulkan::device::destroy_image_view(const gpu::handle<image_view> view) const -> void {
	(*m_device).destroyImageView(std::bit_cast<vk::ImageView>(view), nullptr);
}

auto gse::vulkan::device::create_image_unbound(const gpu::image_create_info& info) const -> std::pair<gpu::handle<image>, gpu::memory_requirements> {
	const vk::ImageCreateInfo vk_info{
		.flags = to_vk(info.flags),
		.imageType = to_vk(info.type),
		.format = to_vk(info.format),
		.extent = vk::Extent3D{ info.extent.x(), info.extent.y(), info.extent.z() },
		.mipLevels = info.mip_levels,
		.arrayLayers = info.array_layers,
		.samples = to_vk(info.samples),
		.tiling = vk::ImageTiling::eOptimal,
		.usage = to_vk(info.usage),
	};
	const auto vk_image = (*m_device).createImage(vk_info, nullptr);
	const auto reqs = (*m_device).getImageMemoryRequirements(vk_image);
	return {
		std::bit_cast<gpu::handle<image>>(vk_image),
		gpu::memory_requirements{
			.size = reqs.size,
			.alignment = reqs.alignment,
			.memory_type_bits = reqs.memoryTypeBits,
		},
	};
}

auto gse::vulkan::device::create_buffer_unbound(const gpu::buffer_create_info& info) const -> std::pair<gpu::handle<buffer>, gpu::memory_requirements> {
	const vk::BufferCreateInfo vk_info{
		.pNext = info.pnext,
		.size = info.size,
		.usage = to_vk(info.usage),
	};
	const auto vk_buffer = (*m_device).createBuffer(vk_info, nullptr);
	const auto reqs = (*m_device).getBufferMemoryRequirements(vk_buffer);
	return {
		std::bit_cast<gpu::handle<buffer>>(vk_buffer),
		gpu::memory_requirements{
			.size = reqs.size,
			.alignment = reqs.alignment,
			.memory_type_bits = reqs.memoryTypeBits,
		},
	};
}

auto gse::vulkan::device::bind_image_memory(const gpu::handle<image> img, const gpu::device_memory_handle mem, const gpu::device_size offset) const -> void {
	(*m_device).bindImageMemory(std::bit_cast<vk::Image>(img), std::bit_cast<vk::DeviceMemory>(mem.value), offset);
}

auto gse::vulkan::device::bind_buffer_memory(const gpu::handle<buffer> buf, const gpu::device_memory_handle mem, const gpu::device_size offset) const -> void {
	(*m_device).bindBufferMemory(std::bit_cast<vk::Buffer>(buf), std::bit_cast<vk::DeviceMemory>(mem.value), offset);
}

auto gse::vulkan::device::create_image_view(const gpu::handle<image> img, const gpu::image_view_create_info& info) const -> gpu::handle<image_view> {
	const vk::ImageViewCreateInfo vk_info{
		.image = std::bit_cast<vk::Image>(img),
		.viewType = to_vk(info.view_type),
		.format = to_vk(info.format),
		.subresourceRange = {
			.aspectMask = to_vk(info.aspects),
			.baseMipLevel = info.base_mip_level,
			.levelCount = info.level_count,
			.baseArrayLayer = info.base_array_layer,
			.layerCount = info.layer_count,
		},
	};
	const auto vk_view = (*m_device).createImageView(vk_info, nullptr);
	return std::bit_cast<gpu::handle<image_view>>(vk_view);
}

auto gse::vulkan::device::allocate_aliased_memory(const gpu::device_size size, const std::uint32_t memory_type_index) const -> gpu::device_memory_handle {
	const vk::MemoryAllocateInfo alloc_info{
		.allocationSize = size,
		.memoryTypeIndex = memory_type_index,
	};
	const auto vk_memory = (*m_device).allocateMemory(alloc_info);
	return {
		.value = std::bit_cast<std::uint64_t>(vk_memory)
	};
}

auto gse::vulkan::device::free_aliased_memory(const gpu::device_memory_handle mem) const -> void {
	if (!mem) {
		return;
	}
	(*m_device).freeMemory(std::bit_cast<vk::DeviceMemory>(mem.value), nullptr);
}

auto gse::vulkan::device::find_memory_type_index(const std::uint32_t type_bits, const gpu::memory_property_flags required) const -> std::uint32_t {
	const auto vk_required = to_vk(required);
	const auto props = m_physical_device.getMemoryProperties();
	for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
		if (!(type_bits & (1u << i))) {
			continue;
		}
		if ((props.memoryTypes[i].propertyFlags & vk_required) != vk_required) {
			continue;
		}
		return i;
	}
	assert(false, "no compatible memory type for type_bits={:#x}", type_bits);
	return 0;
}

auto gse::vulkan::device::free_allocation(const basic_allocation<device>& alloc) -> void {
	std::lock_guard lock(m_mutex);

	if (!alloc.owner()) {
		return;
	}

	if ((m_settings && m_settings->tracking_enabled)) {
		const auto& [creation_location, tag, allocation_id] = alloc.debug_info();
		assert(
			!m_cleaned_up,
			"Allocation freed after allocator cleanup! Tag: '{}', Created at: {}:{}:{}",
			tag,
			creation_location.file_name(),
			creation_location.line(),
			creation_location.function_name()
		);

		if (allocation_id != 0) {
			m_live_allocations.erase(allocation_id);
			--m_live_allocation_count;
		}
	}

	const auto* sub_to_free = alloc.owner();

	for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (auto& block : blocks) {
			for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
				if (&*it == sub_to_free) {
					it->in_use = false;

					if (const auto next_it = std::next(it); next_it != block.allocations.end() && !next_it->in_use) {
						it->size += next_it->size;
						block.allocations.erase(next_it);
					}

					if (it != block.allocations.begin()) {
						if (const auto prev_it = std::prev(it); !prev_it->in_use) {
							prev_it->size += it->size;
							block.allocations.erase(it);
							return;
						}
					}
					return;
				}
			}
		}
	}
}

gse::vulkan::device::device(vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device, device::settings& cfg, const bool device_fault_enabled, const bool device_fault_vendor_binary_enabled, const std::uint32_t graphics_family, const std::uint32_t compute_family)
	: m_physical_device(std::move(physical_device)), m_device(std::move(device)), m_fault_enabled(device_fault_enabled), m_vendor_binary_fault_enabled(device_fault_vendor_binary_enabled), m_settings(&cfg) {
	m_queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)] = graphics_family;
	m_queue_families[static_cast<std::size_t>(gpu::queue_type::compute)] = compute_family;
}

auto gse::vulkan::device::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties, const std::string_view tag, const std::source_location loc, const bool device_address) -> std::expected<basic_allocation<device>, std::string> {
	std::lock_guard lock(m_mutex);

	if ((m_settings && m_settings->tracking_enabled)) {
		assert(
			!m_cleaned_up,
			"Attempted to allocate after allocator cleanup! This indicates a resource lifetime issue."
		);
	}

	const auto mem_props = m_physical_device.getMemoryProperties();

	std::uint32_t new_memory_type_index = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((requirements.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			new_memory_type_index = i;
			break;
		}
	}

	if (new_memory_type_index == std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected{ "Failed to find suitable memory type for allocation!" };
	}

	auto& [memory_type_index, blocks] = m_pools[{ new_memory_type_index, properties, device_address }];
	if (blocks.empty()) {
		memory_type_index = new_memory_type_index;
	}

	memory_block* best_block = nullptr;
	std::list<sub_allocation>::iterator best_sub_it;
	vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
	vk::DeviceSize best_aligned_offset = 0;

	if (!blocks.empty()) {
		for (auto& block : blocks) {
			for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
				if (it->in_use) {
					continue;
				}

				const vk::DeviceSize aligned_offset = (it->offset + requirements.alignment - 1) & ~(requirements.alignment - 1);
				if (aligned_offset + requirements.size > it->offset + it->size) {
					continue;
				}

				if (const vk::DeviceSize leftover = it->size - (aligned_offset - it->offset) - requirements.size; leftover < best_leftover) {
					best_block = &block;
					best_sub_it = it;
					best_leftover = leftover;
					best_aligned_offset = aligned_offset;
					if (leftover == 0) {
						break;
					}
				}
			}
			if (best_leftover == 0) {
				break;
			}
		}
	}

	if (best_block) {
		auto& list = best_block->allocations;
		const auto sub = *best_sub_it;

		const vk::DeviceSize prefix_size = best_aligned_offset - sub.offset;
		const vk::DeviceSize suffix_size = sub.size - prefix_size - requirements.size;

		const auto next_it = list.erase(best_sub_it);
		if (prefix_size) {
			list.insert(
				next_it,
				{ sub.offset, prefix_size, false }
			);
		}
		const auto owner_it = list.insert(
			next_it,
			{ best_aligned_offset, requirements.size, true }
		);
		if (suffix_size) {
			list.insert(
				next_it,
				{ best_aligned_offset + requirements.size, suffix_size, false }
			);
		}

		allocation_debug_info debug_info;
		debug_info.creation_location = loc;
		debug_info.tag = tag;
		if ((m_settings && m_settings->tracking_enabled)) {
			debug_info.allocation_id = m_next_allocation_id++;
			m_live_allocations[debug_info.allocation_id] = debug_info;
			++m_live_allocation_count;
		}

		return basic_allocation<device>{ std::bit_cast<std::uint64_t>(best_block->memory),
										 requirements.size,
										 best_aligned_offset,
										 best_block->mapped
											 ? static_cast<char*>(best_block->mapped) + best_aligned_offset
											 : nullptr,
										 &*owner_it,
										 this,
										 this,
										 std::move(debug_info) };
	}

	const vk::DeviceSize aligned_offset = (0 + requirements.alignment - 1) & ~(requirements.alignment - 1);
	const vk::DeviceSize required_space = aligned_offset + requirements.size;
	const auto new_block_size = std::max(required_space, k_default_block_size);

	const vk::MemoryAllocateFlagsInfo flags_info{
		.flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
	};
	const vk::MemoryAllocateInfo alloc_info{
		.pNext = device_address ? static_cast<const void*>(&flags_info) : nullptr,
		.allocationSize = new_block_size,
		.memoryTypeIndex = new_memory_type_index,
	};
	auto memory = (*m_device).allocateMemory(alloc_info, nullptr);

	blocks.emplace_back(memory, new_block_size, properties);

	auto& new_block = blocks.back();

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = (*m_device).mapMemory(
			new_block.memory,
			0,
			new_block.size,
			{}
		);
	}

	const vk::DeviceSize prefix_size = aligned_offset;
	const vk::DeviceSize suffix_size = new_block.size - required_space;

	if (prefix_size > 0) {
		new_block.allocations.push_back({ 0, prefix_size, false });
	}
	new_block.allocations.push_back({ aligned_offset, requirements.size, true });
	sub_allocation* owner_ptr = &new_block.allocations.back();
	if (suffix_size > 0) {
		new_block.allocations.push_back({ aligned_offset + requirements.size, suffix_size, false });
	}

	allocation_debug_info debug_info;
	debug_info.creation_location = loc;
	debug_info.tag = tag;
	if ((m_settings && m_settings->tracking_enabled)) {
		debug_info.allocation_id = m_next_allocation_id++;
		m_live_allocations[debug_info.allocation_id] = debug_info;
		++m_live_allocation_count;
	}

	return basic_allocation<device>{ std::bit_cast<std::uint64_t>(new_block.memory),
									 requirements.size,
									 aligned_offset,
									 new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
									 owner_ptr,
									 this,
									 this,
									 std::move(debug_info) };
}

auto gse::vulkan::device::clean_up() -> void {
	std::lock_guard lock(m_mutex);

	if (!*m_device) {
		return;
	}

	std::uint32_t leaked_sub_allocations = 0;
	for (const auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (const auto& block : blocks) {
			for (const auto& sub : block.allocations) {
				if (sub.in_use) {
					++leaked_sub_allocations;
				}
			}
		}
	}

	if (leaked_sub_allocations > 0) {
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"{} sub-allocations still in use at allocator cleanup!",
			leaked_sub_allocations
		);
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"Resources are being destroyed after their memory is freed."
		);
		log::println(
			log::level::error,
			log::category::vulkan_memory,
			"Destroy all vulkan::basic_image<vulkan::device> and vulkan::basic_buffer<vulkan::device> instances before "
			"the device."
		);

		if ((m_settings && m_settings->tracking_enabled) && !m_live_allocations.empty()) {
			log::println(log::level::error, log::category::vulkan_memory, "Tracked allocations still alive:");
			for (const auto& [id, debug] : m_live_allocations) {
				log::println(
					log::level::error,
					log::category::vulkan_memory,
					"- [{}] '{}' created at {}:{}:{}",
					id,
					debug.tag.empty() ? "(no tag)" : debug.tag,
					debug.creation_location.file_name(),
					debug.creation_location.line(),
					debug.creation_location.function_name()
				);
			}
		}
		else if (!(m_settings && m_settings->tracking_enabled)) {
			log::println(
				log::level::warning,
				log::category::vulkan_memory,
				"Enable 'Vulkan.Track Allocations' for detailed allocation info."
			);
		}

		assert(
			false,
			"Device cleanup called with {} leaked sub-allocations - destroy resources before the device!",
			leaked_sub_allocations
		);
	}

	m_cleaned_up = true;

	for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
		for (auto& block : blocks) {
			if (block.mapped) {
				(*m_device).unmapMemory(block.memory);
				block.mapped = nullptr;
			}
			if (block.memory) {
				(*m_device).freeMemory(block.memory, nullptr);
			}
		}
	}
	m_pools.clear();
}

auto gse::vulkan::device::memory_flag_preferences(const vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags> {
	using mpf = vk::MemoryPropertyFlagBits;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer || usage & vk::BufferUsageFlagBits::eIndexBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eDeviceLocal,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eUniformBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eHostVisible,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
		return {
			mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eDeviceLocal,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
		return { mpf::eDeviceLocal };
	}

	return { mpf::eHostVisible | mpf::eHostCoherent };
}

auto gse::vulkan::device::pool_key::operator==(const pool_key& other) const -> bool {
	return memory_type_index == other.memory_type_index && properties == other.properties &&
		device_address == other.device_address;
}

auto gse::vulkan::device::pool_key_hash::operator()(const pool_key& key) const noexcept -> std::size_t {
	const auto memory_hash = std::hash<std::uint32_t>()(key.memory_type_index);
	const auto props_hash =
		std::hash<vk::MemoryPropertyFlags::MaskType>()(static_cast<vk::MemoryPropertyFlags::MaskType>(key.properties));
	const auto address_hash = std::hash<bool>()(key.device_address);
	return memory_hash ^ (props_hash << 1) ^ (address_hash << 2);
}
