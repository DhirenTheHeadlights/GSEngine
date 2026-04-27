export module gse.gpu.types;

import std;
import vulkan;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

export namespace gse::gpu {
	enum class buffer_flag : std::uint32_t {
		uniform = 0x01,
		storage = 0x02,
		indirect = 0x04,
		transfer_dst = 0x08,
		vertex = 0x10,
		index = 0x20,
		transfer_src = 0x40,
		acceleration_structure_storage = 0x80,
		acceleration_structure_build_input = 0x100,
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
		r8g8b8_srgb [[= vk::Format::eR8G8B8Srgb]],
		r8g8b8_unorm [[= vk::Format::eR8G8B8Unorm]],
		r8_unorm [[= vk::Format::eR8Unorm]],
		b10g11r11_ufloat [[= vk::Format::eB10G11R11UfloatPack32]],
		r8g8_snorm [[= vk::Format::eR8G8Snorm]],
		r8g8_unorm [[= vk::Format::eR8G8Unorm]],
		r16g16b16a16_sfloat [[= vk::Format::eR16G16B16A16Sfloat]],
	};

	enum class image_view_type : std::uint8_t {
		e2d,
		cube
	};

	enum class image_flag : std::uint8_t {
		sampled          = 1 << 0,
		depth_attachment = 1 << 1,
		color_attachment = 1 << 2,
		transfer_dst     = 1 << 3,
		storage          = 1 << 4,
		transfer_src     = 1 << 5
	};

	using image_usage = gse::flags<image_flag>;
	constexpr auto operator|(image_flag a, image_flag b) -> image_usage { return image_usage(a) | b; }

	enum class image_layout : std::uint8_t {
		undefined [[= vk::ImageLayout::eUndefined]],
		general [[= vk::ImageLayout::eGeneral]],
		shader_read_only [[= vk::ImageLayout::eShaderReadOnlyOptimal]],
	};

	struct image_desc {
		vec2u size = { 1, 1 };
		image_format format = image_format::d32_sfloat;
		image_view_type view = image_view_type::e2d;
		image_usage usage = image_flag::sampled | image_flag::depth_attachment;
		image_layout ready_layout = image_layout::undefined;
	};

	enum class index_type : std::uint8_t {
		uint16,
		uint32
	};

	enum class bind_point : std::uint8_t {
		graphics,
		compute
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
		vertex = 1 << 0,
		fragment = 1 << 1,
		compute = 1 << 2,
		task = 1 << 3,
		mesh = 1 << 4,
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
}
