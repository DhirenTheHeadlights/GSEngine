export module gse.gpu_backend:pipeline;

import std;

import :core;
import :enums;
import :image;

import gse.math;

export namespace gse::gpu {
	struct pipeline_state_cache {
		std::optional<gpu::topology> topology;
		std::optional<gpu::polygon_mode> polygon_mode;
		std::optional<gpu::cull_mode> cull_mode;
		std::optional<gpu::front_face> front_face;
		std::optional<bool> depth_test_enable;
		std::optional<bool> depth_write_enable;
		std::optional<gpu::compare_op> depth_compare_op;
		std::optional<bool> depth_bias_enable;
		std::optional<bool> rasterizer_discard_enable;
		std::optional<bool> primitive_restart_enable;
		std::optional<bool> alpha_to_coverage_enable;
		std::optional<bool> alpha_to_one_enable;
		std::optional<bool> logic_op_enable;
		std::optional<bool> depth_clamp_enable;

		auto invalidate() -> void {
			*this = {};
		}
	};

	struct resource_ref {
		const void* ptr = nullptr;
		resource_type type = resource_type::buffer;
		image_aspect_flags aspects = {};
		device_size buffer_size = 0;
		const void* host_buffer = nullptr;
	};

	struct resource_slot {
		std::uint32_t slot = 0;
		resource_ref ref;
	};

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

	struct descriptor_binding_desc {
		std::uint32_t binding = 0;
		descriptor_type type = descriptor_type::uniform_buffer;
		std::uint32_t count = 1;
		stage_flags stages;
		descriptor_access access = descriptor_access::read;
	};

	struct binding_use {
		std::uint32_t set = 0;
		std::uint32_t slot = 0;
		std::uint32_t count = 1;
		descriptor_access access = descriptor_access::read;
		descriptor_type type = descriptor_type::storage_buffer;
		pipeline_stage_flags stages = {};
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

	struct descriptor_address_info {
		device_address address = 0;
		device_size range = 0;
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

	struct rendering_attachment_info {
		gpu::handle<gpu::image_view> image_view;
		load_op load = load_op::dont_care;
		store_op store = store_op::dont_care;
		color_clear color_clear_value;
		depth_clear depth_clear_value;
	};

	struct rendering_info {
		gse::rect_t<vec2i> render_area;
		std::uint32_t layer_count = 1;
		std::span<const rendering_attachment_info> color_attachments;
		const rendering_attachment_info* depth_attachment = nullptr;
		const rendering_attachment_info* stencil_attachment = nullptr;
		bool secondary_command_buffers = false;
	};
}
