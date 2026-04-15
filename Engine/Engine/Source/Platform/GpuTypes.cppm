export module gse.platform:gpu_types;

import std;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	enum class buffer_flag : std::uint32_t {
		uniform                        = 0x01,
		storage                        = 0x02,
		indirect                       = 0x04,
		transfer_dst                   = 0x08,
		vertex                         = 0x10,
		index                          = 0x20,
		transfer_src                   = 0x40,
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

	enum class cull_mode : std::uint8_t { none, front, back };
	enum class compare_op : std::uint8_t { never, less, equal, less_or_equal, greater, not_equal, greater_or_equal, always };
	enum class polygon_mode : std::uint8_t { fill, line, point };
	enum class topology : std::uint8_t { triangle_list, line_list, point_list };

	enum class blend_preset : std::uint8_t {
		none,
		alpha,
		alpha_premultiplied
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
		nearest,
		linear
	};

	enum class sampler_address_mode : std::uint8_t {
		repeat,
		clamp_to_edge,
		clamp_to_border,
		mirrored_repeat
	};

	enum class border_color : std::uint8_t {
		float_opaque_white,
		float_opaque_black,
		float_transparent_black
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
		r8g8b8_srgb,
		r8g8b8_unorm,
		r8_unorm,
		b10g11r11_ufloat,
		r8g8_snorm,
		r8g8_unorm,
		r16g16b16a16_sfloat
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
		undefined,
		general,
		shader_read_only
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
		vertex   = 1 << 0,
		fragment = 1 << 1,
		compute  = 1 << 2,
		task     = 1 << 3,
		mesh     = 1 << 4,
	};

	using stage_flags = gse::flags<stage_flag>;
	constexpr auto operator|(stage_flag a, stage_flag b) -> stage_flags { return stage_flags(a) | b; }

	enum class descriptor_type : std::uint8_t {
		uniform_buffer,
		storage_buffer,
		combined_image_sampler,
		sampled_image,
		storage_image,
		sampler,
		acceleration_structure
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
		r32g32b32a32_uint
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
