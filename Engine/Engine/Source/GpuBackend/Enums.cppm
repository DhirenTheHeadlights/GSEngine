export module gse.gpu_backend:enums;

import std;

import gse.core;

export namespace gse::gpu {
	enum class backend_kind : std::uint8_t {
		vulkan,
		dx12,
	};

	enum class resource_type : std::uint8_t {
		buffer,
		image,
		acceleration_structure,
	};

	enum class descriptor_access : std::uint8_t {
		read,
		read_write,
	};

	enum class descriptor_type : std::uint8_t {
		uniform_buffer,
		storage_buffer,
		combined_image_sampler,
		sampled_image,
		storage_image,
		sampler,
		acceleration_structure,
	};

	enum class cull_mode : std::uint8_t {
		none,
		front,
		back,
	};

	enum class compare_op : std::uint8_t {
		never,
		less,
		equal,
		less_or_equal,
		greater,
		not_equal,
		greater_or_equal,
		always,
	};

	enum class polygon_mode : std::uint8_t {
		fill,
		line,
		point,
	};

	enum class topology : std::uint8_t {
		triangle_list,
		line_list,
		point_list,
	};

	enum class blend_preset : std::uint8_t {
		none,
		alpha,
		alpha_premultiplied,
	};

	enum class front_face : std::uint8_t {
		counter_clockwise,
		clockwise,
	};

	enum class blend_factor : std::uint8_t {
		zero,
		one,
		src_color,
		one_minus_src_color,
		dst_color,
		one_minus_dst_color,
		src_alpha,
		one_minus_src_alpha,
		dst_alpha,
		one_minus_dst_alpha,
	};

	enum class blend_op : std::uint8_t {
		add,
		subtract,
		reverse_subtract,
		min,
		max,
	};

	enum class color_component_flag : std::uint8_t {
		r = 1 << 0,
		g = 1 << 1,
		b = 1 << 2,
		a = 1 << 3,
	};

	using color_component_flags = flags<color_component_flag>;

	enum class color_format : std::uint8_t {
		swapchain,
		hdr,
		none
	};
	enum class depth_format : std::uint8_t {
		d32_sfloat,
		none
	};

	enum class index_type : std::uint8_t {
		uint16,
		uint32,
	};

	enum class bind_point : std::uint8_t {
		graphics,
		compute,
	};

	enum class queue_type : std::uint8_t {
		graphics,
		compute,
		video_encode,
	};

	constexpr std::uint32_t queue_type_count = 3;

	enum class queue_id : std::uint8_t {
		graphics = 0,
		compute = 1,
	};

	constexpr std::size_t queue_id_count = 2;

	constexpr std::uint32_t max_frames_in_flight = 2;

	enum class frame_status : std::uint8_t {
		minimized,
		swapchain_out_of_date,
		device_lost
	};

	enum class shader_stage : std::uint8_t {
		vertex,
		fragment,
		compute,
		task,
		mesh
	};

	enum class stage_flag : std::uint8_t {
		vertex = 1 << 0,
		fragment = 1 << 1,
		compute = 1 << 2,
		task = 1 << 3,
		mesh = 1 << 4,
	};

	using stage_flags = flags<stage_flag>;

	enum class vertex_format : std::uint8_t {
		r32_sfloat,
		r32g32_sfloat,
		r32g32b32_sfloat,
		r32g32b32a32_sfloat,
		r32_sint,
		r32g32_sint,
		r32g32b32_sint,
		r32g32b32a32_sint,
		r32_uint,
		r32g32_uint,
		r32g32b32_uint,
		r32g32b32a32_uint,
	};

	enum class access_flag : std::uint64_t {
		none = 0,
		indirect_command_read = 1ull << 0,
		index_read = 1ull << 1,
		vertex_attribute_read = 1ull << 2,
		uniform_read = 1ull << 3,
		input_attachment_read = 1ull << 4,
		shader_read = 1ull << 5,
		shader_write = 1ull << 6,
		color_attachment_read = 1ull << 7,
		color_attachment_write = 1ull << 8,
		depth_stencil_attachment_read = 1ull << 9,
		depth_stencil_attachment_write = 1ull << 10,
		transfer_read = 1ull << 11,
		transfer_write = 1ull << 12,
		host_read = 1ull << 13,
		host_write = 1ull << 14,
		memory_read = 1ull << 15,
		memory_write = 1ull << 16,
		shader_sampled_read = 1ull << 17,
		shader_storage_read = 1ull << 18,
		shader_storage_write = 1ull << 19,
		acceleration_structure_read = 1ull << 20,
		acceleration_structure_write = 1ull << 21,
	};

	using access_flags = flags<access_flag>;

	enum class pipeline_stage_flag : std::uint64_t {
		none = 0,
		top_of_pipe = 1ull << 0,
		draw_indirect = 1ull << 1,
		vertex_input = 1ull << 2,
		vertex_shader = 1ull << 3,
		tessellation_control = 1ull << 4,
		tessellation_evaluation = 1ull << 5,
		geometry_shader = 1ull << 6,
		fragment_shader = 1ull << 7,
		early_fragment_tests = 1ull << 8,
		late_fragment_tests = 1ull << 9,
		color_attachment_output = 1ull << 10,
		compute_shader = 1ull << 11,
		transfer = 1ull << 12,
		bottom_of_pipe = 1ull << 13,
		host = 1ull << 14,
		all_graphics = 1ull << 15,
		all_commands = 1ull << 16,
		copy = 1ull << 17,
		resolve = 1ull << 18,
		blit = 1ull << 19,
		clear = 1ull << 20,
		index_input = 1ull << 21,
		vertex_attribute_input = 1ull << 22,
		pre_rasterization_shaders = 1ull << 23,
		mesh_shader = 1ull << 24,
		task_shader = 1ull << 25,
		acceleration_structure_build = 1ull << 26,
		ray_tracing_shader = 1ull << 27,
	};

	using pipeline_stage_flags = flags<pipeline_stage_flag>;

	enum class memory_property_flag : std::uint32_t {
		device_local = 1u << 0,
		host_visible = 1u << 1,
		host_coherent = 1u << 2,
		host_cached = 1u << 3,
		lazily_allocated = 1u << 4,
	};

	using memory_property_flags = flags<memory_property_flag>;

	enum class present_mode_setting : int {
		fifo = 0,
		fifo_relaxed = 1,
		mailbox = 2,
		immediate = 3,
	};

	enum class present_mode : std::uint8_t {
		immediate [[= present_mode_setting::immediate]],
		mailbox [[= present_mode_setting::mailbox]],
		fifo [[= present_mode_setting::fifo]],
		fifo_relaxed [[= present_mode_setting::fifo_relaxed]],
	};

	enum class present_stage_flag : std::uint32_t {
		queue_operations_end = 1u << 0,
		request_dequeued = 1u << 1,
		image_first_pixel_out = 1u << 2,
		image_first_pixel_visible = 1u << 3,
	};

	using present_stage_flags = flags<present_stage_flag>;

	enum class load_op : std::uint8_t {
		load,
		clear,
		dont_care,
	};

	enum class store_op : std::uint8_t {
		store,
		dont_care,
	};

	enum class pipeline_statistic_flag : std::uint32_t {
		input_assembly_vertices = 1u << 0,
		input_assembly_primitives = 1u << 1,
		clipping_invocations = 1u << 2,
		fragment_shader_invocations = 1u << 3,
	};

	using pipeline_statistic_flags = flags<pipeline_statistic_flag>;

	enum class query_status : std::uint8_t {
		success,
		error,
	};

	enum class sample_count : std::uint8_t {
		e1,
		e2,
		e4,
		e8,
		e16,
		e32,
		e64,
	};
}