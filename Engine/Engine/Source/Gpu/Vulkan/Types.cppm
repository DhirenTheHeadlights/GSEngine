export module gse.gpu:types;

import std;
import vulkan;
import gse.std_meta;

import :handles;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	struct descriptor_buffer_properties {
		vk::DeviceSize offset_alignment = 0;
		vk::DeviceSize uniform_buffer_descriptor_size = 0;
		vk::DeviceSize storage_buffer_descriptor_size = 0;
		vk::DeviceSize sampled_image_descriptor_size = 0;
		vk::DeviceSize sampler_descriptor_size = 0;
		vk::DeviceSize combined_image_sampler_descriptor_size = 0;
		vk::DeviceSize storage_image_descriptor_size = 0;
		vk::DeviceSize input_attachment_descriptor_size = 0;
		vk::DeviceSize acceleration_structure_descriptor_size = 0;
		bool push_descriptors_supported = false;
		bool bufferless_push_descriptors = false;
	};

	enum class buffer_flag : std::uint32_t {
		uniform [[= vk::BufferUsageFlagBits::eUniformBuffer]] [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x01,
		storage [[= vk::BufferUsageFlagBits::eStorageBuffer]] [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x02,
		indirect [[= vk::BufferUsageFlagBits::eIndirectBuffer]] [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x04,
		transfer_dst [[= vk::BufferUsageFlagBits::eTransferDst]] = 0x08,
		vertex [[= vk::BufferUsageFlagBits::eVertexBuffer]] = 0x10,
		index [[= vk::BufferUsageFlagBits::eIndexBuffer]] = 0x20,
		transfer_src [[= vk::BufferUsageFlagBits::eTransferSrc]] = 0x40,
		acceleration_structure_storage [[= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR]] [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x80,
		acceleration_structure_build_input [[= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR]] [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x100,
	};

	using buffer_usage = gse::flags<buffer_flag>;
	constexpr auto operator|(buffer_flag a, buffer_flag b) -> buffer_usage { return buffer_usage(a) | b; }

	struct buffer_desc {
		std::size_t size = 0;
		buffer_usage usage = buffer_flag::storage;
		const void* data = nullptr;
	};

	enum class cull_mode : std::uint8_t {
		none [[= vk::CullModeFlagBits::eNone]],
		front [[= vk::CullModeFlagBits::eFront]],
		back [[= vk::CullModeFlagBits::eBack]],
	};

	enum class compare_op : std::uint8_t {
		never [[= vk::CompareOp::eNever]],
		less [[= vk::CompareOp::eLess]],
		equal [[= vk::CompareOp::eEqual]],
		less_or_equal [[= vk::CompareOp::eLessOrEqual]],
		greater [[= vk::CompareOp::eGreater]],
		not_equal [[= vk::CompareOp::eNotEqual]],
		greater_or_equal [[= vk::CompareOp::eGreaterOrEqual]],
		always [[= vk::CompareOp::eAlways]],
	};

	enum class polygon_mode : std::uint8_t {
		fill [[= vk::PolygonMode::eFill]],
		line [[= vk::PolygonMode::eLine]],
		point [[= vk::PolygonMode::ePoint]],
	};

	enum class topology : std::uint8_t {
		triangle_list [[= vk::PrimitiveTopology::eTriangleList]],
		line_list [[= vk::PrimitiveTopology::eLineList]],
		point_list [[= vk::PrimitiveTopology::ePointList]],
	};

	enum class blend_preset : std::uint8_t {
		none,
		alpha,
		alpha_premultiplied,
	};

	struct depth_state {
		bool test = true;
		bool write = true;
		compare_op compare = compare_op::less;
	};

	struct rasterization_state {
		polygon_mode polygon = polygon_mode::fill;
		cull_mode cull = cull_mode::back;
		float line_width = 1.0f;
		bool depth_bias = false;
		float depth_bias_constant = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
	};

	enum class color_format : std::uint8_t {
		swapchain,
		none
	};
	enum class depth_format : std::uint8_t {
		d32_sfloat,
		none
	};

	enum class sampler_filter : std::uint8_t {
		nearest [[= vk::Filter::eNearest]],
		linear [[= vk::Filter::eLinear]],
	};

	enum class sampler_address_mode : std::uint8_t {
		repeat [[= vk::SamplerAddressMode::eRepeat]],
		clamp_to_edge [[= vk::SamplerAddressMode::eClampToEdge]],
		clamp_to_border [[= vk::SamplerAddressMode::eClampToBorder]],
		mirrored_repeat [[= vk::SamplerAddressMode::eMirroredRepeat]],
	};

	enum class border_color : std::uint8_t {
		float_opaque_white [[= vk::BorderColor::eFloatOpaqueWhite]],
		float_opaque_black [[= vk::BorderColor::eFloatOpaqueBlack]],
		float_transparent_black [[= vk::BorderColor::eFloatTransparentBlack]],
	};

	struct sampler_desc {
		sampler_filter min = sampler_filter::linear;
		sampler_filter mag = sampler_filter::linear;
		sampler_address_mode address_u = sampler_address_mode::repeat;
		sampler_address_mode address_v = sampler_address_mode::repeat;
		sampler_address_mode address_w = sampler_address_mode::repeat;
		bool compare_enable = false;
		compare_op compare = compare_op::always;
		border_color border = border_color::float_opaque_white;
		float max_anisotropy = 0.0f;
		float min_lod = 0.0f;
		float max_lod = 0.0f;
	};

	enum class image_format : std::uint8_t {
		d32_sfloat [[= vk::Format::eD32Sfloat]],
		r8g8b8a8_srgb [[= vk::Format::eR8G8B8A8Srgb]],
		r8g8b8a8_unorm [[= vk::Format::eR8G8B8A8Unorm]],
		b8g8r8a8_srgb [[= vk::Format::eB8G8R8A8Srgb]],
		b8g8r8a8_unorm [[= vk::Format::eB8G8R8A8Unorm]],
		r8g8b8_srgb [[= vk::Format::eR8G8B8Srgb]],
		r8g8b8_unorm [[= vk::Format::eR8G8B8Unorm]],
		r8_unorm [[= vk::Format::eR8Unorm]],
		b10g11r11_ufloat [[= vk::Format::eB10G11R11UfloatPack32]],
		r8g8_snorm [[= vk::Format::eR8G8Snorm]],
		r8g8_unorm [[= vk::Format::eR8G8Unorm]],
		r16g16b16a16_sfloat [[= vk::Format::eR16G16B16A16Sfloat]],
	};

	enum class image_view_type : std::uint8_t {
		e2d [[= vk::ImageViewType::e2D]],
		cube [[= vk::ImageViewType::eCube]],
	};

	enum class image_flag : std::uint8_t {
		sampled [[= vk::ImageUsageFlagBits::eSampled]] = 1 << 0,
		depth_attachment [[= vk::ImageUsageFlagBits::eDepthStencilAttachment]] = 1 << 1,
		color_attachment [[= vk::ImageUsageFlagBits::eColorAttachment]] = 1 << 2,
		transfer_dst [[= vk::ImageUsageFlagBits::eTransferDst]] = 1 << 3,
		storage [[= vk::ImageUsageFlagBits::eStorage]] = 1 << 4,
		transfer_src [[= vk::ImageUsageFlagBits::eTransferSrc]] = 1 << 5,
	};

	using image_usage = gse::flags<image_flag>;
	constexpr auto operator|(image_flag a, image_flag b) -> image_usage { return image_usage(a) | b; }

	enum class image_layout : std::uint8_t {
		undefined [[= vk::ImageLayout::eUndefined]],
		general [[= vk::ImageLayout::eGeneral]],
		shader_read_only [[= vk::ImageLayout::eShaderReadOnlyOptimal]],
		color_attachment [[= vk::ImageLayout::eColorAttachmentOptimal]],
		depth_stencil_attachment [[= vk::ImageLayout::eDepthStencilAttachmentOptimal]],
		depth_attachment [[= vk::ImageLayout::eDepthAttachmentOptimal]],
		transfer_src [[= vk::ImageLayout::eTransferSrcOptimal]],
		transfer_dst [[= vk::ImageLayout::eTransferDstOptimal]],
		present_src [[= vk::ImageLayout::ePresentSrcKHR]],
		video_encode_src [[= vk::ImageLayout::eVideoEncodeSrcKHR]],
	};

	struct image_desc {
		vec2u size = { 1, 1 };
		image_format format = image_format::d32_sfloat;
		image_view_type view = image_view_type::e2d;
		image_usage usage = image_flag::sampled | image_flag::depth_attachment;
		image_layout ready_layout = image_layout::undefined;
	};

	enum class index_type : std::uint8_t {
		uint16 [[= vk::IndexType::eUint16]],
		uint32 [[= vk::IndexType::eUint32]],
	};

	enum class bind_point : std::uint8_t {
		graphics [[= vk::PipelineBindPoint::eGraphics]],
		compute [[= vk::PipelineBindPoint::eCompute]],
	};

	enum class pipeline_stage : std::uint8_t {
		vertex_shader,
		fragment_shader,
		compute_shader,
		draw_indirect,
		late_fragment_tests,
	};

	enum class barrier_scope : std::uint8_t {
		compute_to_compute,
		compute_to_indirect,
		host_to_compute,
		transfer_to_compute,
		compute_to_transfer,
		transfer_to_host,
		transfer_to_transfer
	};

	enum class frame_status : std::uint8_t {
		minimized,
		swapchain_out_of_date,
		device_lost
	};

	struct frame_token {
		std::uint32_t frame_index = 0;
		std::uint32_t image_index = 0;
	};

	struct draw_indexed_indirect_command {
		std::uint32_t index_count;
		std::uint32_t instance_count;
		std::uint32_t first_index;
		std::int32_t  vertex_offset;
		std::uint32_t first_instance;
	};

	struct color_clear {
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	};

	struct depth_clear {
		float depth = 1.0f;
	};

	enum class shader_stage : std::uint8_t {
		vertex, fragment, compute, task, mesh
	};

	enum class stage_flag : std::uint8_t {
		vertex [[= vk::ShaderStageFlagBits::eVertex]] = 1 << 0,
		fragment [[= vk::ShaderStageFlagBits::eFragment]] = 1 << 1,
		compute [[= vk::ShaderStageFlagBits::eCompute]] = 1 << 2,
		task [[= vk::ShaderStageFlagBits::eTaskEXT]] = 1 << 3,
		mesh [[= vk::ShaderStageFlagBits::eMeshEXT]] = 1 << 4,
	};

	using stage_flags = gse::flags<stage_flag>;
	constexpr auto operator|(stage_flag a, stage_flag b) -> stage_flags { return stage_flags(a) | b; }

	enum class descriptor_type : std::uint8_t {
		uniform_buffer [[= vk::DescriptorType::eUniformBuffer]],
		storage_buffer [[= vk::DescriptorType::eStorageBuffer]],
		combined_image_sampler [[= vk::DescriptorType::eCombinedImageSampler]],
		sampled_image [[= vk::DescriptorType::eSampledImage]],
		storage_image [[= vk::DescriptorType::eStorageImage]],
		sampler [[= vk::DescriptorType::eSampler]],
		acceleration_structure [[= vk::DescriptorType::eAccelerationStructureKHR]],
	};

	enum class vertex_format : std::uint8_t {
		r32_sfloat [[= vk::Format::eR32Sfloat]],
		r32g32_sfloat [[= vk::Format::eR32G32Sfloat]],
		r32g32b32_sfloat [[= vk::Format::eR32G32B32Sfloat]],
		r32g32b32a32_sfloat [[= vk::Format::eR32G32B32A32Sfloat]],
		r32_sint [[= vk::Format::eR32Sint]],
		r32g32_sint [[= vk::Format::eR32G32Sint]],
		r32g32b32_sint [[= vk::Format::eR32G32B32Sint]],
		r32g32b32a32_sint [[= vk::Format::eR32G32B32A32Sint]],
		r32_uint [[= vk::Format::eR32Uint]],
		r32g32_uint [[= vk::Format::eR32G32Uint]],
		r32g32b32_uint [[= vk::Format::eR32G32B32Uint]],
		r32g32b32a32_uint [[= vk::Format::eR32G32B32A32Uint]],
	};

	struct vertex_binding_desc {
		std::uint32_t binding = 0;
		std::uint32_t stride = 0;
		bool per_instance = false;
	};

	struct vertex_attribute_desc {
		std::uint32_t location = 0;
		std::uint32_t binding = 0;
		vertex_format format = vertex_format::r32_sfloat;
		std::uint32_t offset = 0;
	};

	struct descriptor_binding_desc {
		std::uint32_t binding = 0;
		descriptor_type type = descriptor_type::uniform_buffer;
		std::uint32_t count = 1;
		stage_flags stages;
	};

	enum class descriptor_set_type : std::uint8_t {
		persistent = 0,
		push = 1,
		bind_less = 2
	};

	struct acceleration_structure_handle {
		std::uint64_t value = 0;

		explicit operator bool() const { return value != 0; }
	};

	enum class acceleration_structure_type : std::uint8_t {
		top_level [[= vk::AccelerationStructureTypeKHR::eTopLevel]],
		bottom_level [[= vk::AccelerationStructureTypeKHR::eBottomLevel]],
	};

	enum class build_acceleration_structure_mode : std::uint8_t {
		build [[= vk::BuildAccelerationStructureModeKHR::eBuild]],
		update [[= vk::BuildAccelerationStructureModeKHR::eUpdate]],
	};

	enum class build_acceleration_structure_flag : std::uint32_t {
		allow_update [[= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate]] = 1u << 0,
		allow_compaction [[= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction]] = 1u << 1,
		prefer_fast_trace [[= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace]] = 1u << 2,
		prefer_fast_build [[= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild]] = 1u << 3,
		low_memory [[= vk::BuildAccelerationStructureFlagBitsKHR::eLowMemory]] = 1u << 4,
	};

	using build_acceleration_structure_flags = gse::flags<build_acceleration_structure_flag>;
	constexpr auto operator|(build_acceleration_structure_flag a, build_acceleration_structure_flag b) -> build_acceleration_structure_flags { return build_acceleration_structure_flags(a) | b; }

	enum class geometry_flag : std::uint8_t {
		opaque [[= vk::GeometryFlagBitsKHR::eOpaque]] = 1 << 0,
		no_duplicate_any_hit_invocation [[= vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation]] = 1 << 1,
	};

	using geometry_flags = gse::flags<geometry_flag>;
	constexpr auto operator|(geometry_flag a, geometry_flag b) -> geometry_flags { return geometry_flags(a) | b; }

	struct acceleration_structure_geometry_triangles_data {
		vertex_format vertex_format = vertex_format::r32g32b32_sfloat;
		device_address vertex_data = 0;
		device_size vertex_stride = 0;
		std::uint32_t max_vertex = 0;
		index_type index_type = index_type::uint32;
		device_address index_data = 0;
	};

	struct acceleration_structure_geometry_instances_data {
		bool array_of_pointers = false;
		device_address data = 0;
	};

	enum class acceleration_structure_geometry_type : std::uint8_t {
		triangles,
		instances,
	};

	struct acceleration_structure_geometry {
		acceleration_structure_geometry_type type = acceleration_structure_geometry_type::instances;
		acceleration_structure_geometry_triangles_data triangles;
		acceleration_structure_geometry_instances_data instances;
		geometry_flags flags;
	};

	struct acceleration_structure_build_geometry_info {
		acceleration_structure_type type = acceleration_structure_type::bottom_level;
		build_acceleration_structure_flags flags;
		build_acceleration_structure_mode mode = build_acceleration_structure_mode::build;
		acceleration_structure_handle dst;
		std::span<const acceleration_structure_geometry> geometries;
		device_address scratch_address = 0;
	};

	struct acceleration_structure_build_range_info {
		std::uint32_t primitive_count = 0;
		std::uint32_t primitive_offset = 0;
		std::uint32_t first_vertex = 0;
		std::uint32_t transform_offset = 0;
	};

	enum class result : std::int32_t {
		success [[= vk::Result::eSuccess]],
		not_ready [[= vk::Result::eNotReady]],
		timeout [[= vk::Result::eTimeout]],
		event_set [[= vk::Result::eEventSet]],
		event_reset [[= vk::Result::eEventReset]],
		incomplete [[= vk::Result::eIncomplete]],
		suboptimal_khr [[= vk::Result::eSuboptimalKHR]],
		error_out_of_host_memory [[= vk::Result::eErrorOutOfHostMemory]],
		error_out_of_device_memory [[= vk::Result::eErrorOutOfDeviceMemory]],
		error_device_lost [[= vk::Result::eErrorDeviceLost]],
		error_out_of_date_khr [[= vk::Result::eErrorOutOfDateKHR]],
		error_surface_lost_khr [[= vk::Result::eErrorSurfaceLostKHR]],
		error_unknown [[= vk::Result::eErrorUnknown]],
	};

	enum class image_aspect_flag : std::uint32_t {
		color [[= vk::ImageAspectFlagBits::eColor]] = 1u << 0,
		depth [[= vk::ImageAspectFlagBits::eDepth]] = 1u << 1,
		stencil [[= vk::ImageAspectFlagBits::eStencil]] = 1u << 2,
	};

	using image_aspect_flags = gse::flags<image_aspect_flag>;
	constexpr auto operator|(image_aspect_flag a, image_aspect_flag b) -> image_aspect_flags { return image_aspect_flags(a) | b; }

	enum class access_flag : std::uint64_t {
		none = 0,
		indirect_command_read [[= vk::AccessFlagBits2::eIndirectCommandRead]] = 1ull << 0,
		index_read [[= vk::AccessFlagBits2::eIndexRead]] = 1ull << 1,
		vertex_attribute_read [[= vk::AccessFlagBits2::eVertexAttributeRead]] = 1ull << 2,
		uniform_read [[= vk::AccessFlagBits2::eUniformRead]] = 1ull << 3,
		input_attachment_read [[= vk::AccessFlagBits2::eInputAttachmentRead]] = 1ull << 4,
		shader_read [[= vk::AccessFlagBits2::eShaderRead]] = 1ull << 5,
		shader_write [[= vk::AccessFlagBits2::eShaderWrite]] = 1ull << 6,
		color_attachment_read [[= vk::AccessFlagBits2::eColorAttachmentRead]] = 1ull << 7,
		color_attachment_write [[= vk::AccessFlagBits2::eColorAttachmentWrite]] = 1ull << 8,
		depth_stencil_attachment_read [[= vk::AccessFlagBits2::eDepthStencilAttachmentRead]] = 1ull << 9,
		depth_stencil_attachment_write [[= vk::AccessFlagBits2::eDepthStencilAttachmentWrite]] = 1ull << 10,
		transfer_read [[= vk::AccessFlagBits2::eTransferRead]] = 1ull << 11,
		transfer_write [[= vk::AccessFlagBits2::eTransferWrite]] = 1ull << 12,
		host_read [[= vk::AccessFlagBits2::eHostRead]] = 1ull << 13,
		host_write [[= vk::AccessFlagBits2::eHostWrite]] = 1ull << 14,
		memory_read [[= vk::AccessFlagBits2::eMemoryRead]] = 1ull << 15,
		memory_write [[= vk::AccessFlagBits2::eMemoryWrite]] = 1ull << 16,
		shader_sampled_read [[= vk::AccessFlagBits2::eShaderSampledRead]] = 1ull << 17,
		shader_storage_read [[= vk::AccessFlagBits2::eShaderStorageRead]] = 1ull << 18,
		shader_storage_write [[= vk::AccessFlagBits2::eShaderStorageWrite]] = 1ull << 19,
		acceleration_structure_read [[= vk::AccessFlagBits2::eAccelerationStructureReadKHR]] = 1ull << 20,
		acceleration_structure_write [[= vk::AccessFlagBits2::eAccelerationStructureWriteKHR]] = 1ull << 21,
	};

	using access_flags = gse::flags<access_flag>;
	constexpr auto operator|(access_flag a, access_flag b) -> access_flags { return access_flags(a) | b; }

	enum class pipeline_stage_flag : std::uint64_t {
		none = 0,
		top_of_pipe [[= vk::PipelineStageFlagBits2::eTopOfPipe]] = 1ull << 0,
		draw_indirect [[= vk::PipelineStageFlagBits2::eDrawIndirect]] = 1ull << 1,
		vertex_input [[= vk::PipelineStageFlagBits2::eVertexInput]] = 1ull << 2,
		vertex_shader [[= vk::PipelineStageFlagBits2::eVertexShader]] = 1ull << 3,
		tessellation_control [[= vk::PipelineStageFlagBits2::eTessellationControlShader]] = 1ull << 4,
		tessellation_evaluation [[= vk::PipelineStageFlagBits2::eTessellationEvaluationShader]] = 1ull << 5,
		geometry_shader [[= vk::PipelineStageFlagBits2::eGeometryShader]] = 1ull << 6,
		fragment_shader [[= vk::PipelineStageFlagBits2::eFragmentShader]] = 1ull << 7,
		early_fragment_tests [[= vk::PipelineStageFlagBits2::eEarlyFragmentTests]] = 1ull << 8,
		late_fragment_tests [[= vk::PipelineStageFlagBits2::eLateFragmentTests]] = 1ull << 9,
		color_attachment_output [[= vk::PipelineStageFlagBits2::eColorAttachmentOutput]] = 1ull << 10,
		compute_shader [[= vk::PipelineStageFlagBits2::eComputeShader]] = 1ull << 11,
		transfer [[= vk::PipelineStageFlagBits2::eAllTransfer]] = 1ull << 12,
		bottom_of_pipe [[= vk::PipelineStageFlagBits2::eBottomOfPipe]] = 1ull << 13,
		host [[= vk::PipelineStageFlagBits2::eHost]] = 1ull << 14,
		all_graphics [[= vk::PipelineStageFlagBits2::eAllGraphics]] = 1ull << 15,
		all_commands [[= vk::PipelineStageFlagBits2::eAllCommands]] = 1ull << 16,
		copy [[= vk::PipelineStageFlagBits2::eCopy]] = 1ull << 17,
		resolve [[= vk::PipelineStageFlagBits2::eResolve]] = 1ull << 18,
		blit [[= vk::PipelineStageFlagBits2::eBlit]] = 1ull << 19,
		clear [[= vk::PipelineStageFlagBits2::eClear]] = 1ull << 20,
		index_input [[= vk::PipelineStageFlagBits2::eIndexInput]] = 1ull << 21,
		vertex_attribute_input [[= vk::PipelineStageFlagBits2::eVertexAttributeInput]] = 1ull << 22,
		pre_rasterization_shaders [[= vk::PipelineStageFlagBits2::ePreRasterizationShaders]] = 1ull << 23,
		mesh_shader [[= vk::PipelineStageFlagBits2::eMeshShaderEXT]] = 1ull << 24,
		task_shader [[= vk::PipelineStageFlagBits2::eTaskShaderEXT]] = 1ull << 25,
		acceleration_structure_build [[= vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR]] = 1ull << 26,
		ray_tracing_shader [[= vk::PipelineStageFlagBits2::eRayTracingShaderKHR]] = 1ull << 27,
	};

	using pipeline_stage_flags = gse::flags<pipeline_stage_flag>;
	constexpr auto operator|(pipeline_stage_flag a, pipeline_stage_flag b) -> pipeline_stage_flags { return pipeline_stage_flags(a) | b; }

	enum class memory_property_flag : std::uint32_t {
		device_local [[= vk::MemoryPropertyFlagBits::eDeviceLocal]] = 1u << 0,
		host_visible [[= vk::MemoryPropertyFlagBits::eHostVisible]] = 1u << 1,
		host_coherent [[= vk::MemoryPropertyFlagBits::eHostCoherent]] = 1u << 2,
		host_cached [[= vk::MemoryPropertyFlagBits::eHostCached]] = 1u << 3,
		lazily_allocated [[= vk::MemoryPropertyFlagBits::eLazilyAllocated]] = 1u << 4,
	};

	using memory_property_flags = gse::flags<memory_property_flag>;
	constexpr auto operator|(memory_property_flag a, memory_property_flag b) -> memory_property_flags { return memory_property_flags(a) | b; }

	enum class present_mode : std::uint8_t {
		immediate [[= vk::PresentModeKHR::eImmediate]],
		mailbox [[= vk::PresentModeKHR::eMailbox]],
		fifo [[= vk::PresentModeKHR::eFifo]],
		fifo_relaxed [[= vk::PresentModeKHR::eFifoRelaxed]],
	};

	enum class color_space : std::uint8_t {
		srgb_nonlinear [[= vk::ColorSpaceKHR::eSrgbNonlinear]],
	};

	struct surface_format {
		image_format format = image_format::r8g8b8a8_srgb;
		color_space color_space = color_space::srgb_nonlinear;
	};

	struct surface_capabilities {
		std::uint32_t min_image_count = 0;
		std::uint32_t max_image_count = 0;
		vec2u current_extent;
		vec2u min_image_extent;
		vec2u max_image_extent;
		std::uint32_t max_image_array_layers = 0;
	};

	struct viewport {
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float min_depth = 0.0f;
		float max_depth = 1.0f;
	};

	struct push_constant_range {
		stage_flags stages;
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	struct buffer_copy_region {
		device_size src_offset = 0;
		device_size dst_offset = 0;
		device_size size = 0;
	};

	struct image_subresource_layers {
		image_aspect_flags aspects;
		std::uint32_t mip_level = 0;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct buffer_image_copy_region {
		device_size buffer_offset = 0;
		std::uint32_t buffer_row_length = 0;
		std::uint32_t buffer_image_height = 0;
		image_subresource_layers image_subresource;
		vec3i image_offset;
		vec3u image_extent;
	};

	struct image_copy_region {
		image_subresource_layers src_subresource;
		vec3i src_offset;
		image_subresource_layers dst_subresource;
		vec3i dst_offset;
		vec3u extent;
	};

	struct image_blit_region {
		image_subresource_layers src_subresource;
		vec3i src_offsets[2];
		image_subresource_layers dst_subresource;
		vec3i dst_offsets[2];
	};

	struct shader_stage_create_info {
		stage_flag stage = stage_flag::vertex;
		std::uint64_t module_handle = 0;
		std::string entry_point = "main";
	};

	struct memory_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
	};

	struct descriptor_address_info {
		device_address address = 0;
		device_size range = 0;
	};

	enum class load_op : std::uint8_t {
		load [[= vk::AttachmentLoadOp::eLoad]],
		clear [[= vk::AttachmentLoadOp::eClear]],
		dont_care [[= vk::AttachmentLoadOp::eDontCare]],
	};

	enum class store_op : std::uint8_t {
		store [[= vk::AttachmentStoreOp::eStore]],
		dont_care [[= vk::AttachmentStoreOp::eDontCare]],
	};

	struct buffer_create_info {
		device_size size = 0;
		buffer_usage usage = buffer_flag::storage;
	};

	enum class image_create_flag : std::uint8_t {
		cube_compatible [[= vk::ImageCreateFlagBits::eCubeCompatible]] = 1 << 0,
	};

	using image_create_flags = gse::flags<image_create_flag>;
	constexpr auto operator|(image_create_flag a, image_create_flag b) -> image_create_flags { return image_create_flags(a) | b; }

	enum class image_type : std::uint8_t {
		e1d [[= vk::ImageType::e1D]],
		e2d [[= vk::ImageType::e2D]],
		e3d [[= vk::ImageType::e3D]],
	};

	enum class sample_count : std::uint8_t {
		e1 [[= vk::SampleCountFlagBits::e1]],
		e2 [[= vk::SampleCountFlagBits::e2]],
		e4 [[= vk::SampleCountFlagBits::e4]],
		e8 [[= vk::SampleCountFlagBits::e8]],
		e16 [[= vk::SampleCountFlagBits::e16]],
		e32 [[= vk::SampleCountFlagBits::e32]],
		e64 [[= vk::SampleCountFlagBits::e64]],
	};

	struct image_create_info {
		image_create_flags flags;
		image_type type = image_type::e2d;
		image_format format = image_format::r8g8b8a8_unorm;
		vec3u extent;
		std::uint32_t mip_levels = 1;
		std::uint32_t array_layers = 1;
		sample_count samples = sample_count::e1;
		image_usage usage;
	};

	struct image_view_create_info {
		image_format format = image_format::r8g8b8a8_unorm;
		image_view_type view_type = image_view_type::e2d;
		image_aspect_flags aspects;
		std::uint32_t base_mip_level = 0;
		std::uint32_t level_count = 1;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct device_fault_counts {
		std::uint32_t address_info_count = 0;
		std::uint32_t vendor_info_count = 0;
		std::size_t vendor_binary_size = 0;
	};

	struct device_fault_address_info {
		std::uint32_t address_type = 0;
		device_address reported_address = 0;
		device_address address_precision = 0;
	};

	struct device_fault_vendor_info {
		std::string description;
		std::uint64_t vendor_fault_code = 0;
		std::uint64_t vendor_fault_data = 0;
	};

	struct device_fault_info {
		std::string description;
		std::span<device_fault_address_info> address_infos;
		std::span<device_fault_vendor_info> vendor_infos;
		std::span<std::byte> vendor_binary;
	};

}

export namespace gse::vulkan {
	consteval auto first_annotation_of_type(
		std::meta::info enumerator,
		std::meta::info vk_type
	) -> std::meta::info;

	template <typename E>
	consteval auto first_annotated_enumerator(
	) -> std::meta::info;

	template <typename E>
	consteval auto enum_has_vk_annotation(
	) -> bool;

	template <typename E>
	consteval auto bitflag_enum_has_vk_annotation(
	) -> bool;

	template <typename Vk, typename E>
		requires std::is_enum_v<E>
	auto reflect_to_vk(
		E e
	) -> Vk;

	template <typename E, typename Vk>
		requires std::is_enum_v<E>
	auto reflect_from_vk(
		Vk v,
		E fallback = E{}
	) -> E;

	template <typename E>
	using vk_type_of = [: std::meta::type_of(std::meta::annotations_of(std::meta::enumerators_of(^^E)[0])[0]) :];

	template <typename E>
	using vk_flag_bit_type_of = [: std::meta::type_of(std::meta::annotations_of(first_annotated_enumerator<E>())[0]) :];

	template <typename FlagEnum>
	consteval auto collect_flag_annotations(
	) -> std::vector<std::pair<FlagEnum, vk_flag_bit_type_of<FlagEnum>>>;

	consteval auto snake_to_camel(
		std::string_view snake
	) -> std::string;
}

consteval auto gse::vulkan::first_annotation_of_type(const std::meta::info enumerator, const std::meta::info vk_type) -> std::meta::info {
	for (std::meta::info ann : std::meta::annotations_of(enumerator)) {
		if (std::meta::type_of(ann) == vk_type) {
			return ann;
		}
	}
	return {};
}

template <typename E>
consteval auto gse::vulkan::enum_has_vk_annotation() -> bool {
	if constexpr (std::is_enum_v<E>) {
		auto enums = std::meta::enumerators_of(^^E);
		if (enums.empty()) {
			return false;
		}
		return !std::meta::annotations_of(enums[0]).empty();
	}
	return false;
}

template <typename E>
consteval auto gse::vulkan::first_annotated_enumerator() -> std::meta::info {
	if constexpr (std::is_enum_v<E>) {
		for (auto e : std::meta::enumerators_of(^^E)) {
			if (!std::meta::annotations_of(e).empty()) {
				return e;
			}
		}
	}
	return {};
}

template <typename E>
consteval auto gse::vulkan::bitflag_enum_has_vk_annotation() -> bool {
	if constexpr (std::is_enum_v<E>) {
		for (auto e : std::meta::enumerators_of(^^E)) {
			if (!std::meta::annotations_of(e).empty()) {
				return true;
			}
		}
	}
	return false;
}

template <typename FlagEnum>
consteval auto gse::vulkan::collect_flag_annotations() -> std::vector<std::pair<FlagEnum, vk_flag_bit_type_of<FlagEnum>>> {
	using vk_bits = vk_flag_bit_type_of<FlagEnum>;
	std::vector<std::pair<FlagEnum, vk_bits>> result;
	for (auto e : std::meta::enumerators_of(^^FlagEnum)) {
		for (auto a : std::meta::annotations_of(e)) {
			result.emplace_back(std::meta::extract<FlagEnum>(e), std::meta::extract<vk_bits>(a));
		}
	}
	return result;
}

consteval auto gse::vulkan::snake_to_camel(const std::string_view snake) -> std::string {
	std::string result;
	bool capitalize = false;
	for (char c : snake) {
		if (c == '_') {
			capitalize = true;
		} else if (capitalize) {
			result += static_cast<char>(c - 'a' + 'A');
			capitalize = false;
		} else {
			result += c;
		}
	}
	return result;
}

template <typename Vk, typename E>
	requires std::is_enum_v<E>
auto gse::vulkan::reflect_to_vk(const E e) -> Vk {
	template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if ([:v:] == e) {
			return std::meta::extract<Vk>(first_annotation_of_type(v, ^^Vk));
		}
	}
	std::unreachable();
}

template <typename E, typename Vk>
	requires std::is_enum_v<E>
auto gse::vulkan::reflect_from_vk(const Vk v, const E fallback) -> E {
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if (std::meta::extract<Vk>(first_annotation_of_type(e, ^^Vk)) == v) {
			return [:e:];
		}
	}
	return fallback;
}

export namespace gse::vulkan {
	template <typename E>
		requires (enum_has_vk_annotation<E>())
	auto to_vk(
		E e
	) -> vk_type_of<E>;

	template <typename FlagEnum>
		requires (bitflag_enum_has_vk_annotation<FlagEnum>())
	auto to_vk(
		gse::flags<FlagEnum> fls
	) -> vk::Flags<vk_flag_bit_type_of<FlagEnum>>;

	template <typename Dst, typename Src>
	auto reflect_struct_to_vk(
		const Src& src
	) -> Dst;

	auto to_vk(
		const gpu::surface_format& sf
	) -> vk::SurfaceFormatKHR;

	auto to_vk(
		const gpu::viewport& v
	) -> vk::Viewport;

	auto to_vk(
		const gpu::push_constant_range& pcr
	) -> vk::PushConstantRange;

	auto to_vk(
		const gpu::buffer_copy_region& r
	) -> vk::BufferCopy;

	auto to_vk(
		const gpu::image_subresource_layers& s
	) -> vk::ImageSubresourceLayers;

	auto to_vk(
		const gpu::buffer_image_copy_region& r
	) -> vk::BufferImageCopy;

	auto to_vk(
		const gpu::image_copy_region& r
	) -> vk::ImageCopy;

	auto to_vk(
		const gpu::image_blit_region& r
	) -> vk::ImageBlit;

	auto to_vk(
		const gpu::acceleration_structure_build_range_info& r
	) -> vk::AccelerationStructureBuildRangeInfoKHR;

	auto from_vk(
		vk::Result r
	) -> gpu::result;

	auto from_vk(
		vk::PresentModeKHR mode
	) -> gpu::present_mode;

	auto from_vk(
		vk::ColorSpaceKHR cs
	) -> gpu::color_space;

	auto from_vk(
		vk::Format f
	) -> gpu::image_format;

	auto from_vk(
		const vk::SurfaceFormatKHR& sf
	) -> gpu::surface_format;

	auto from_vk(
		const vk::SurfaceCapabilitiesKHR& caps
	) -> gpu::surface_capabilities;
}

template <typename E>
	requires (gse::vulkan::enum_has_vk_annotation<E>())
auto gse::vulkan::to_vk(const E e) -> vk_type_of<E> {
	return reflect_to_vk<vk_type_of<E>>(e);
}

template <typename FlagEnum>
	requires (gse::vulkan::bitflag_enum_has_vk_annotation<FlagEnum>())
auto gse::vulkan::to_vk(const gse::flags<FlagEnum> fls) -> vk::Flags<vk_flag_bit_type_of<FlagEnum>> {
	using vk_bits = vk_flag_bit_type_of<FlagEnum>;
	vk::Flags<vk_bits> result{};
	constexpr auto pairs = std::define_static_array(collect_flag_annotations<FlagEnum>());
	for (const auto& [bit, vk_value] : pairs) {
		if (fls.test(bit)) {
			result |= vk_value;
		}
	}
	return result;
}

template <typename Dst, typename Src>
auto gse::vulkan::reflect_struct_to_vk(const Src& src) -> Dst {
	Dst out{};
	template for (constexpr auto src_member : std::define_static_array(std::meta::nonstatic_data_members_of(^^Src, std::meta::access_context::unchecked()))) {
		constexpr auto dst_member = []() consteval {
			auto camel = snake_to_camel(std::meta::identifier_of(src_member));
			for (auto m : std::meta::nonstatic_data_members_of(^^Dst, std::meta::access_context::unchecked())) {
				if (std::meta::identifier_of(m) == camel) {
					return m;
				}
			}
			return std::meta::info{};
		}();
		using src_type = [: std::meta::type_of(src_member) :];
		using dst_type = [: std::meta::type_of(dst_member) :];
		if constexpr (std::is_same_v<src_type, dst_type>) {
			out.[: dst_member :] = src.[: src_member :];
		} else {
			out.[: dst_member :] = to_vk(src.[: src_member :]);
		}
	}
	return out;
}

auto gse::vulkan::to_vk(const gpu::surface_format& sf) -> vk::SurfaceFormatKHR {
	return reflect_struct_to_vk<vk::SurfaceFormatKHR>(sf);
}

auto gse::vulkan::to_vk(const gpu::viewport& v) -> vk::Viewport {
	return reflect_struct_to_vk<vk::Viewport>(v);
}

auto gse::vulkan::to_vk(const gpu::push_constant_range& pcr) -> vk::PushConstantRange {
	return {
		.stageFlags = to_vk(pcr.stages),
		.offset = pcr.offset,
		.size = pcr.size,
	};
}

auto gse::vulkan::to_vk(const gpu::buffer_copy_region& r) -> vk::BufferCopy {
	return reflect_struct_to_vk<vk::BufferCopy>(r);
}

auto gse::vulkan::to_vk(const gpu::image_subresource_layers& s) -> vk::ImageSubresourceLayers {
	return {
		.aspectMask = to_vk(s.aspects),
		.mipLevel = s.mip_level,
		.baseArrayLayer = s.base_array_layer,
		.layerCount = s.layer_count,
	};
}

auto gse::vulkan::to_vk(const gpu::buffer_image_copy_region& r) -> vk::BufferImageCopy {
	return {
		.bufferOffset = r.buffer_offset,
		.bufferRowLength = r.buffer_row_length,
		.bufferImageHeight = r.buffer_image_height,
		.imageSubresource = to_vk(r.image_subresource),
		.imageOffset = vk::Offset3D{ r.image_offset.x(), r.image_offset.y(), r.image_offset.z() },
		.imageExtent = vk::Extent3D{ r.image_extent.x(), r.image_extent.y(), r.image_extent.z() },
	};
}

auto gse::vulkan::to_vk(const gpu::image_copy_region& r) -> vk::ImageCopy {
	return {
		.srcSubresource = to_vk(r.src_subresource),
		.srcOffset = vk::Offset3D{ r.src_offset.x(), r.src_offset.y(), r.src_offset.z() },
		.dstSubresource = to_vk(r.dst_subresource),
		.dstOffset = vk::Offset3D{ r.dst_offset.x(), r.dst_offset.y(), r.dst_offset.z() },
		.extent = vk::Extent3D{ r.extent.x(), r.extent.y(), r.extent.z() },
	};
}

auto gse::vulkan::to_vk(const gpu::image_blit_region& r) -> vk::ImageBlit {
	vk::ImageBlit out{
		.srcSubresource = to_vk(r.src_subresource),
		.dstSubresource = to_vk(r.dst_subresource),
	};
	out.srcOffsets[0] = vk::Offset3D{ r.src_offsets[0].x(), r.src_offsets[0].y(), r.src_offsets[0].z() };
	out.srcOffsets[1] = vk::Offset3D{ r.src_offsets[1].x(), r.src_offsets[1].y(), r.src_offsets[1].z() };
	out.dstOffsets[0] = vk::Offset3D{ r.dst_offsets[0].x(), r.dst_offsets[0].y(), r.dst_offsets[0].z() };
	out.dstOffsets[1] = vk::Offset3D{ r.dst_offsets[1].x(), r.dst_offsets[1].y(), r.dst_offsets[1].z() };
	return out;
}

auto gse::vulkan::to_vk(const gpu::acceleration_structure_build_range_info& r) -> vk::AccelerationStructureBuildRangeInfoKHR {
	return reflect_struct_to_vk<vk::AccelerationStructureBuildRangeInfoKHR>(r);
}

auto gse::vulkan::from_vk(const vk::Result r) -> gpu::result {
	return reflect_from_vk<gpu::result>(r, gpu::result::error_unknown);
}

auto gse::vulkan::from_vk(const vk::PresentModeKHR mode) -> gpu::present_mode {
	return reflect_from_vk<gpu::present_mode>(mode, gpu::present_mode::fifo);
}

auto gse::vulkan::from_vk(const vk::ColorSpaceKHR cs) -> gpu::color_space {
	return reflect_from_vk<gpu::color_space>(cs, gpu::color_space::srgb_nonlinear);
}

auto gse::vulkan::from_vk(const vk::Format f) -> gpu::image_format {
	return reflect_from_vk<gpu::image_format>(f, gpu::image_format::r8g8b8a8_unorm);
}

auto gse::vulkan::from_vk(const vk::SurfaceFormatKHR& sf) -> gpu::surface_format {
	return {
		.format = from_vk(sf.format),
		.color_space = from_vk(sf.colorSpace),
	};
}

auto gse::vulkan::from_vk(const vk::SurfaceCapabilitiesKHR& caps) -> gpu::surface_capabilities {
	return {
		.min_image_count = caps.minImageCount,
		.max_image_count = caps.maxImageCount,
		.current_extent = vec2u{ caps.currentExtent.width, caps.currentExtent.height },
		.min_image_extent = vec2u{ caps.minImageExtent.width, caps.minImageExtent.height },
		.max_image_extent = vec2u{ caps.maxImageExtent.width, caps.maxImageExtent.height },
		.max_image_array_layers = caps.maxImageArrayLayers,
	};
}
