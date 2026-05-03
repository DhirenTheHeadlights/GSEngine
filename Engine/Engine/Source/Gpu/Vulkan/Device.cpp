module gse.gpu;

import std;
import vulkan;

import gse.assert;
import gse.core;
import gse.log;
import gse.save;

auto gse::vulkan::transition_image_layout(basic_image<device>& image_resource, const gpu::handle<command_buffer> cmd_handle, const gpu::image_layout new_layout, const gpu::image_aspect_flags aspects, const gpu::pipeline_stage_flags src_stages, const gpu::access_flags src_access, const gpu::pipeline_stage_flags dst_stages, const gpu::access_flags dst_access, const std::uint32_t mip_levels, const std::uint32_t layer_count) -> void {
	const auto current = image_resource.layout();
	if (current == new_layout) {
		return;
	}

	const gpu::image_barrier barrier{
		.src_stages = src_stages,
		.src_access = src_access,
		.dst_stages = dst_stages,
		.dst_access = dst_access,
		.old_layout = current,
		.new_layout = new_layout,
		.image = image_resource.handle(),
		.aspects = aspects,
		.base_mip_level = 0,
		.level_count = mip_levels,
		.base_array_layer = 0,
		.layer_count = layer_count,
	};

	const gpu::dependency_info dep{ .image_barriers = std::span(&barrier, 1) };
	commands(cmd_handle).pipeline_barrier(dep);

	image_resource.set_layout(new_layout);
}

auto gse::vulkan::query_descriptor_buffer_properties(const vk::raii::PhysicalDevice& physical_device) -> gpu::descriptor_buffer_properties {
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

auto gse::vulkan::wait_for_fence(const device& dev, const gpu::handle<fence> fence, const std::uint64_t timeout_ns) -> gpu::result {
	const auto vk_fence = std::bit_cast<vk::Fence>(fence);
	const auto vk_result = dev.raii_device().waitForFences(vk_fence, vk::True, timeout_ns);
	return from_vk(vk_result);
}

auto gse::vulkan::reset_fence(const device& dev, const gpu::handle<fence> fence) -> void {
	dev.raii_device().resetFences(std::bit_cast<vk::Fence>(fence));
}

auto gse::vulkan::acquire_next_image(const device& dev, const gpu::handle<swap_chain> swapchain, const gpu::handle<semaphore> wait_semaphore, const std::uint64_t timeout_ns) -> acquire_next_image_result {
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
	: m_physical_device(std::move(other.m_physical_device)),
	m_device(std::move(other.m_device)),
	m_fault_enabled(other.m_fault_enabled),
	m_vendor_binary_fault_enabled(other.m_vendor_binary_fault_enabled),
	m_pools(std::move(other.m_pools)),
	m_live_allocation_count(other.m_live_allocation_count.load()),
	m_next_allocation_id(other.m_next_allocation_id.load()),
	m_cleaned_up(other.m_cleaned_up),
	m_tracking_enabled(other.m_tracking_enabled),
	m_name_resources(other.m_name_resources),
	m_live_allocations(std::move(other.m_live_allocations)) {}

auto gse::vulkan::device::operator=(device&& other) noexcept -> device& {
	if (this != &other) {
		clean_up();
		m_physical_device = std::move(other.m_physical_device);
		m_device = std::move(other.m_device);
		m_fault_enabled = other.m_fault_enabled;
		m_vendor_binary_fault_enabled = other.m_vendor_binary_fault_enabled;
		m_pools = std::move(other.m_pools);
		m_live_allocation_count = other.m_live_allocation_count.load();
		m_next_allocation_id = other.m_next_allocation_id.load();
		m_cleaned_up = other.m_cleaned_up;
		m_tracking_enabled = other.m_tracking_enabled;
		m_name_resources = other.m_name_resources;
		m_live_allocations = std::move(other.m_live_allocations);
	}
	return *this;
}

auto gse::vulkan::device::create(const instance& instance_data, save::state& save) -> device_creation_result {
	const auto devices = instance_data.enumerate_physical_devices();
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
		families.video_encode_family.has_value() &&
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

	vk::raii::Device logical_device = physical_device.createDevice(create_info);

	vk::detail::defaultDispatchLoaderDynamic.init(*logical_device);

	log::println(log::category::vulkan, "Logical Device Created Successfully!");

	vk::raii::Queue graphics_queue = logical_device.getQueue(families.graphics_family.value(), 0);
	vk::raii::Queue present_queue = logical_device.getQueue(families.present_family.value(), 0);
	vk::raii::Queue compute_queue = logical_device.getQueue(families.compute_family.value(), 0);

	auto desc_buf_props = query_descriptor_buffer_properties(physical_device);
	desc_buf_props.push_descriptors_supported = descriptor_buffer_push_descriptors_supported;

	auto result = device_creation_result{
		.device = device(
			std::move(physical_device),
			std::move(logical_device),
			save,
			device_fault_supported,
			device_fault_vendor_binary_supported
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
		.desc_buf_props = std::move(desc_buf_props),
	};

	if (video_encode_extensions_available) {
		result.queue.set_video_encode(result.device.raii_device().getQueue(families.video_encode_family.value(), 0), families.video_encode_family.value());
		log::println(log::category::vulkan, "Video encode queue acquired (family {})", families.video_encode_family.value());
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
	constexpr auto device_addressable_usage =
		vk::BufferUsageFlagBits::eUniformBuffer
		| vk::BufferUsageFlagBits::eStorageBuffer
		| vk::BufferUsageFlagBits::eIndirectBuffer
		| vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
		| vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
		| vk::BufferUsageFlagBits::eShaderDeviceAddress;
	const bool needs_device_address = static_cast<bool>(actual_buffer_info.usage & device_addressable_usage);

	if (needs_device_address) {
		actual_buffer_info.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}

	auto buffer = (*m_device).createBuffer(actual_buffer_info, nullptr);
	const auto requirements = (*m_device).getBufferMemoryRequirements(buffer);

	basic_allocation<device> alloc{};
	bool success = false;

	if (data) {
		constexpr vk::MemoryPropertyFlags required_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
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

	assert(success, std::source_location::current(), "Failed to allocate memory for buffer after trying all preferences.");

	(*m_device).bindBufferMemory(buffer, std::bit_cast<vk::DeviceMemory>(alloc.memory()), alloc.offset());

	if (m_name_resources) {
		const auto& debug_info = alloc.debug_info();
		const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
		const auto name = debug_info.tag.empty()
			? std::format("Buffer ({}:{})", file, debug_info.creation_location.line())
			: std::format("Buffer '{}' ({}:{})", debug_info.tag, file, debug_info.creation_location.line());

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
		assert(false, std::source_location::current(), "Buffer created with data, but the allocated memory is not mappable.");
	}

	return basic_buffer<device>(std::bit_cast<gpu::handle<vulkan::buffer>>(buffer), std::move(alloc), actual_buffer_info.size);
}

auto gse::vulkan::device::create_buffer(const gpu::buffer_create_info& buffer_info, const void* data, const std::string_view tag, const std::source_location& loc) -> basic_buffer<device> {
	const vk::BufferCreateInfo vk_info{
		.size = buffer_info.size,
		.usage = to_vk(buffer_info.usage),
	};
	return create_buffer(vk_info, data, tag, loc);
}

auto gse::vulkan::device::create_image(const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, const vk::ImageViewCreateInfo& view_info, const void* data, const std::string_view tag, const std::source_location loc) -> basic_image<device> {
	auto image = (*m_device).createImage(info, nullptr);
	const auto requirements = (*m_device).getImageMemoryRequirements(image);

	auto expected_alloc = allocate(requirements, properties, tag, loc);

	if (!expected_alloc) {
		assert(
			false,
			std::source_location::current(),
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
		assert(!view_info.image, std::source_location::current(), "Image view info must not have an image set yet!");
		vk::ImageViewCreateInfo actual_view_info = view_info;
		actual_view_info.image = image;
		view = (*m_device).createImageView(actual_view_info, nullptr);
	}
	else {
		view = (*m_device).createImageView({
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
		}, nullptr);
	}

	return basic_image<device>(
		std::bit_cast<gpu::handle<vulkan::image>>(image),
		std::bit_cast<gpu::handle<vulkan::image_view>>(view),
		static_cast<gpu::image_format_value>(info.format),
		from_vk(info.initialLayout),
		vec3u{ info.extent.width, info.extent.height, info.extent.depth },
		std::move(alloc));
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
	return create_image(vk_info, to_vk(properties), vk_view_info, data, tag, loc);
}

auto gse::vulkan::device::live_allocation_count() const -> std::uint32_t {
	return m_live_allocation_count.load();
}

auto gse::vulkan::device::tracking_enabled() const -> bool {
	return m_tracking_enabled;
}

auto gse::vulkan::device::destroy_buffer(const gpu::handle<buffer> buffer) const -> void {
	(*m_device).destroyBuffer(std::bit_cast<vk::Buffer>(buffer), nullptr);
}

auto gse::vulkan::device::destroy_image(const gpu::handle<image> image) const -> void {
	(*m_device).destroyImage(std::bit_cast<vk::Image>(image), nullptr);
}

auto gse::vulkan::device::destroy_image_view(const gpu::handle<image_view> view) const -> void {
	(*m_device).destroyImageView(std::bit_cast<vk::ImageView>(view), nullptr);
}

auto gse::vulkan::device::free_allocation(const basic_allocation<device>& alloc) -> void {
	std::lock_guard lock(m_mutex);

	if (!alloc.owner()) {
		return;
	}

	if (m_tracking_enabled) {
		const auto& [creation_location, tag, allocation_id] = alloc.debug_info();
		assert(
			!m_cleaned_up,
			std::source_location::current(),
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

gse::vulkan::device::device(vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device, save::state& save_state, const bool device_fault_enabled, const bool device_fault_vendor_binary_enabled)
	: m_physical_device(std::move(physical_device)),
	m_device(std::move(device)),
	m_fault_enabled(device_fault_enabled),
	m_vendor_binary_fault_enabled(device_fault_vendor_binary_enabled) {
	save_state.bind("Vulkan", "Track Allocations", m_tracking_enabled)
		.description("Track Vulkan memory allocations for debugging destruction order issues")
		.default_value(false)
		.commit();

	save_state.bind("Vulkan", "Name Resources", m_name_resources)
		.description("Assign debug names to Vulkan buffers to improve validation output")
		.default_value(false)
		.commit();
}

auto gse::vulkan::device::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties, const std::string_view tag, const std::source_location loc, const bool device_address) -> std::expected<basic_allocation<device>, std::string> {
	std::lock_guard lock(m_mutex);

	if (m_tracking_enabled) {
		assert(
			!m_cleaned_up,
			std::source_location::current(),
			"Attempted to allocate after allocator cleanup! This indicates a resource lifetime issue."
		);
	}

	const auto mem_props = m_physical_device.getMemoryProperties();

	std::uint32_t new_memory_type_index = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((requirements.memoryTypeBits & (1u << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
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
			list.insert(next_it, { sub.offset, prefix_size, false });
		}
		const auto owner_it = list.insert(next_it, { best_aligned_offset, requirements.size, true });
		if (suffix_size) {
			list.insert(next_it, { best_aligned_offset + requirements.size, suffix_size, false });
		}

		allocation_debug_info debug_info;
		debug_info.creation_location = loc;
		debug_info.tag = tag;
		if (m_tracking_enabled) {
			debug_info.allocation_id = m_next_allocation_id++;
			m_live_allocations[debug_info.allocation_id] = debug_info;
			++m_live_allocation_count;
		}

		return basic_allocation<device>{
			std::bit_cast<std::uint64_t>(best_block->memory),
			requirements.size,
			best_aligned_offset,
			best_block->mapped ? static_cast<char*>(best_block->mapped) + best_aligned_offset : nullptr,
			&*owner_it,
			this,
			this,
			std::move(debug_info)
		};
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

	blocks.emplace_back(
		memory,
		new_block_size,
		properties
	);

	auto& new_block = blocks.back();

	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		new_block.mapped = (*m_device).mapMemory(new_block.memory, 0, new_block.size, {});
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
	if (m_tracking_enabled) {
		debug_info.allocation_id = m_next_allocation_id++;
		m_live_allocations[debug_info.allocation_id] = debug_info;
		++m_live_allocation_count;
	}

	return basic_allocation<device>{
		std::bit_cast<std::uint64_t>(new_block.memory),
		requirements.size,
		aligned_offset,
		new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
		owner_ptr,
		this,
		this,
		std::move(debug_info)
	};
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
		log::println(log::level::error, log::category::vulkan_memory, "{} sub-allocations still in use at allocator cleanup!", leaked_sub_allocations);
		log::println(log::level::error, log::category::vulkan_memory, "Resources are being destroyed after their memory is freed.");
		log::println(log::level::error, log::category::vulkan_memory, "Destroy all vulkan::basic_image<vulkan::device> and vulkan::basic_buffer<vulkan::device> instances before the device.");

		if (m_tracking_enabled && !m_live_allocations.empty()) {
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
		} else if (!m_tracking_enabled) {
			log::println(log::level::warning, log::category::vulkan_memory, "Enable 'Vulkan.Track Allocations' for detailed allocation info.");
		}

		assert(
			false,
			std::source_location::current(),
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
			mpf::eHostVisible | mpf::eHostCoherent,
			mpf::eHostVisible,
		};
	}

	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
		return {
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
	return memory_type_index == other.memory_type_index
		&& properties == other.properties
		&& device_address == other.device_address;
}

auto gse::vulkan::device::pool_key_hash::operator()(const pool_key& key) const noexcept -> std::size_t {
	const auto memory_hash = std::hash<std::uint32_t>()(key.memory_type_index);
	const auto props_hash = std::hash<vk::MemoryPropertyFlags::MaskType>()(static_cast<vk::MemoryPropertyFlags::MaskType>(key.properties));
	const auto address_hash = std::hash<bool>()(key.device_address);
	return memory_hash ^ (props_hash << 1) ^ (address_hash << 2);
}
