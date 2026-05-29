export module gse.gpu_types;

import std;

import gse.core;
import gse.math;

export namespace gse::gpu {
	template <typename T>
	struct handle {
		std::uint64_t value = 0;

		constexpr auto operator==(
			const handle&
		) const -> bool = default;

		constexpr auto operator<=>(
			const handle&
		) const = default;

		explicit constexpr operator bool() const {
			return value != 0;
		}
	};

	using device_size = std::uint64_t;
	using device_address = std::uint64_t;
	using image_format_value = std::uint32_t;

	constexpr device_size whole_size = ~static_cast<device_size>(0);

	struct buffer_tag {};
	struct image_tag {};
	struct image_view_tag {};
	struct sampler_tag {};
	struct semaphore_tag {};
	struct fence_tag {};
	struct command_buffer_tag {};
	struct queue_tag {};
	struct swap_chain_tag {};
	struct query_pool_tag {};
	struct shader_object_tag {};
	struct pipeline_layout_tag {};
	struct device_tag {};
	struct physical_device_tag {};
	struct surface_tag {};

	using buffer_handle = handle<buffer_tag>;
	using image_handle = handle<image_tag>;
	using image_view_handle = handle<image_view_tag>;
	using sampler_handle = handle<sampler_tag>;
	using semaphore_handle = handle<semaphore_tag>;
	using fence_handle = handle<fence_tag>;
	using command_buffer_handle = handle<command_buffer_tag>;
	using queue_handle = handle<queue_tag>;
	using swap_chain_handle = handle<swap_chain_tag>;
	using query_pool_handle = handle<query_pool_tag>;
	using shader_object_handle = handle<shader_object_tag>;
	using pipeline_layout_handle = handle<pipeline_layout_tag>;
	using device_handle = handle<device_tag>;
	using physical_device_handle = handle<physical_device_tag>;
	using surface = handle<surface_tag>;

	enum class resource_type : std::uint8_t {
		buffer,
		image,
		acceleration_structure,
	};

	struct resource_ref {
		const void* ptr = nullptr;
		resource_type type = resource_type::buffer;
	};

	enum class descriptor_access : std::uint8_t {
		read,
		read_write,
	};

	struct resource_slot {
		std::uint32_t slot = 0;
		resource_ref ref;
	};

	enum class descriptor_type : std::uint8_t;

	enum class buffer_flag : std::uint32_t {
		uniform = 0x01,
		storage = 0x02,
		indirect = 0x04,
		transfer_dst = 0x08,
		index = 0x20,
		transfer_src = 0x40,
		acceleration_structure_storage = 0x80,
		acceleration_structure_build_input = 0x100,
		video_encode_dst = 0x200,
	};

	using buffer_usage = gse::flags<buffer_flag>;
	constexpr auto operator|(buffer_flag a, buffer_flag b) -> buffer_usage {
		return buffer_usage(a) | b;
	}

	struct buffer_desc {
		device_size size = 0;
		buffer_usage usage = buffer_flag::storage;
		const void* data = nullptr;
		const void* pnext = nullptr;
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

	using color_component_flags = gse::flags<color_component_flag>;
	constexpr auto operator|(color_component_flag a, color_component_flag b) -> color_component_flags {
		return color_component_flags(a) | b;
	}

	struct color_blend_equation {
		blend_factor src_color = blend_factor::one;
		blend_factor dst_color = blend_factor::zero;
		blend_op color_op = blend_op::add;
		blend_factor src_alpha = blend_factor::one;
		blend_factor dst_alpha = blend_factor::zero;
		blend_op alpha_op = blend_op::add;

		constexpr auto operator==(
			const color_blend_equation&
		) const -> bool = default;
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
		hdr,
		none
	};
	enum class depth_format : std::uint8_t {
		d32_sfloat,
		none
	};

	enum class sampler_filter : std::uint8_t {
		nearest,
		linear,
	};

	enum class sampler_address_mode : std::uint8_t {
		repeat,
		clamp_to_edge,
		clamp_to_border,
		mirrored_repeat,
	};

	enum class border_color : std::uint8_t {
		float_opaque_white,
		float_opaque_black,
		float_transparent_black,
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
		d32_sfloat,
		r8g8b8a8_srgb,
		r8g8b8a8_unorm,
		b8g8r8a8_srgb,
		b8g8r8a8_unorm,
		r8g8b8_srgb,
		r8g8b8_unorm,
		r8_unorm,
		b10g11r11_ufloat,
		r8g8_snorm,
		r8g8_unorm,
		r16g16b16a16_sfloat,
		r16g16_sfloat,
	};

	enum class image_view_type : std::uint8_t {
		e2d,
		e3d,
		cube,
	};

	enum class image_flag : std::uint8_t {
		sampled = 1 << 0,
		depth_attachment = 1 << 1,
		color_attachment = 1 << 2,
		transfer_dst = 1 << 3,
		storage = 1 << 4,
		transfer_src = 1 << 5,
		host_transfer = 1 << 6,
	};

	using image_usage = gse::flags<image_flag>;
	constexpr auto operator|(image_flag a, image_flag b) -> image_usage {
		return image_usage(a) | b;
	}

	struct image_desc {
		vec2u size = { 1, 1 };
		std::uint32_t depth = 1;
		image_format format = image_format::d32_sfloat;
		image_view_type view = image_view_type::e2d;
		image_usage usage = image_flag::sampled | image_flag::depth_attachment;
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
	};

	constexpr std::uint32_t queue_type_count = 2;

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
		std::int32_t vertex_offset;
		std::uint32_t first_instance;
	};

	struct draw_mesh_tasks_indirect_command {
		std::uint32_t group_count_x;
		std::uint32_t group_count_y;
		std::uint32_t group_count_z;
	};

	struct color_clear {
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	};

	struct depth_clear {
		float depth = 1.0f;
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

	using stage_flags = gse::flags<stage_flag>;
	constexpr auto operator|(stage_flag a, stage_flag b) -> stage_flags {
		return stage_flags(a) | b;
	}

	enum class descriptor_type : std::uint8_t {
		uniform_buffer,
		storage_buffer,
		combined_image_sampler,
		sampled_image,
		storage_image,
		sampler,
		acceleration_structure,
	};

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

	struct descriptor_binding_desc {
		std::uint32_t binding = 0;
		descriptor_type type = descriptor_type::uniform_buffer;
		std::uint32_t count = 1;
		stage_flags stages;
		descriptor_access access = descriptor_access::read;
	};

	struct acceleration_structure {
		std::uint64_t value = 0;

		explicit operator bool() const {
			return value != 0;
		}
	};

	enum class acceleration_structure_type : std::uint8_t {
		top_level,
		bottom_level,
	};

	enum class build_acceleration_structure_mode : std::uint8_t {
		build,
		update,
	};

	enum class build_acceleration_structure_flag : std::uint32_t {
		allow_update = 1u << 0,
		allow_compaction = 1u << 1,
		prefer_fast_trace = 1u << 2,
		prefer_fast_build = 1u << 3,
		low_memory = 1u << 4,
	};

	using build_acceleration_structure_flags = gse::flags<build_acceleration_structure_flag>;
	constexpr auto operator|(build_acceleration_structure_flag a, build_acceleration_structure_flag b) -> build_acceleration_structure_flags {
		return build_acceleration_structure_flags(a) | b;
	}

	enum class geometry_flag : std::uint8_t {
		opaque = 1 << 0,
		no_duplicate_any_hit_invocation = 1 << 1,
	};

	using geometry_flags = gse::flags<geometry_flag>;
	constexpr auto operator|(geometry_flag a, geometry_flag b) -> geometry_flags {
		return geometry_flags(a) | b;
	}

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
		acceleration_structure dst;
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
		success,
		not_ready,
		timeout,
		event_set,
		event_reset,
		incomplete,
		suboptimal_khr,
		error_out_of_host_memory,
		error_out_of_device_memory,
		error_device_lost,
		error_out_of_date_khr,
		error_surface_lost_khr,
		error_unknown,
	};

	enum class image_aspect_flag : std::uint32_t {
		color = 1u << 0,
		depth = 1u << 1,
		stencil = 1u << 2,
	};

	using image_aspect_flags = gse::flags<image_aspect_flag>;
	constexpr auto operator|(image_aspect_flag a, image_aspect_flag b) -> image_aspect_flags {
		return image_aspect_flags(a) | b;
	}

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

	using access_flags = gse::flags<access_flag>;
	constexpr auto operator|(access_flag a, access_flag b) -> access_flags {
		return access_flags(a) | b;
	}

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

	using pipeline_stage_flags = gse::flags<pipeline_stage_flag>;
	constexpr auto operator|(pipeline_stage_flag a, pipeline_stage_flag b) -> pipeline_stage_flags {
		return pipeline_stage_flags(a) | b;
	}

	struct binding_use {
		std::uint32_t set = 0;
		std::uint32_t slot = 0;
		std::uint32_t count = 1;
		descriptor_access access = descriptor_access::read;
		descriptor_type type = descriptor_type::storage_buffer;
		pipeline_stage_flags stages = {};
	};

	enum class memory_property_flag : std::uint32_t {
		device_local = 1u << 0,
		host_visible = 1u << 1,
		host_coherent = 1u << 2,
		host_cached = 1u << 3,
		lazily_allocated = 1u << 4,
	};

	using memory_property_flags = gse::flags<memory_property_flag>;
	constexpr auto operator|(memory_property_flag a, memory_property_flag b) -> memory_property_flags {
		return memory_property_flags(a) | b;
	}

	enum class present_mode : std::uint8_t {
		immediate,
		mailbox,
		fifo,
		fifo_relaxed,
	};

	enum class color_space : std::uint8_t {
		srgb_nonlinear,
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
		load,
		clear,
		dont_care,
	};

	enum class pipeline_statistic_flag : std::uint32_t {
		input_assembly_vertices = 1u << 0,
		input_assembly_primitives = 1u << 1,
		clipping_invocations = 1u << 2,
		fragment_shader_invocations = 1u << 3,
	};

	using pipeline_statistic_flags = gse::flags<pipeline_statistic_flag>;
	constexpr auto operator|(pipeline_statistic_flag a, pipeline_statistic_flag b) -> pipeline_statistic_flags {
		return pipeline_statistic_flags(a) | b;
	}

	enum class query_status : std::uint8_t {
		success,
		error,
	};

	struct secondary_inheritance_info {
		bool render_pass_continue = false;
		std::span<const image_format_value> color_attachment_formats;
		image_format_value depth_attachment_format = 0;
		pipeline_statistic_flags pipeline_statistics;
	};

	enum class store_op : std::uint8_t {
		store,
		dont_care,
	};

	enum class image_create_flag : std::uint8_t {
		cube_compatible = 1 << 0,
	};

	using image_create_flags = gse::flags<image_create_flag>;
	constexpr auto operator|(image_create_flag a, image_create_flag b) -> image_create_flags {
		return image_create_flags(a) | b;
	}

	enum class image_type : std::uint8_t {
		e1d,
		e2d,
		e3d,
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

	struct dynamic_pipeline_state {
		topology topology = topology::triangle_list;
		polygon_mode polygon = polygon_mode::fill;
		cull_mode cull = cull_mode::back;
		front_face front = front_face::counter_clockwise;
		depth_state depth;
		bool depth_bias_enable = false;
		float depth_bias_constant = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
		bool rasterizer_discard_enable = false;
		bool primitive_restart_enable = false;
		bool depth_clamp_enable = false;
		bool alpha_to_coverage_enable = false;
		bool alpha_to_one_enable = false;
		bool logic_op_enable = false;
		sample_count samples = sample_count::e1;
		std::uint32_t sample_mask = 0xFFFFFFFFu;
		std::vector<std::uint8_t> blend_enables;
		std::vector<color_blend_equation> blend_equations;
		std::vector<color_component_flags> color_write_masks;
	};

	struct memory_requirements {
		device_size size = 0;
		device_size alignment = 0;
		std::uint32_t memory_type_bits = 0;
	};

	struct device_memory {
		std::uint64_t value = 0;

		constexpr auto operator==(
			const device_memory&
		) const -> bool = default;

		explicit constexpr operator bool() const {
			return value != 0;
		}
	};

	struct acquire_next_image_result {
		gse::gpu::result result = gse::gpu::result::error_unknown;
		std::uint32_t image_index = 0;
	};

	struct acceleration_structure_build_sizes {
		device_size acceleration_structure_size = 0;
		device_size build_scratch_size = 0;
		device_size update_scratch_size = 0;
	};
}
