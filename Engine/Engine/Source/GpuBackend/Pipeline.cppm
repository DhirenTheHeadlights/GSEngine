export module gse.gpu_backend:pipeline;

import std;

import :core;
import :enums;

export namespace gse::gpu {
	struct resource_ref {
		const void* ptr = nullptr;
		resource_type type = resource_type::buffer;
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

	struct secondary_inheritance_info {
		bool render_pass_continue = false;
		std::span<const image_format_value> color_attachment_formats;
		image_format_value depth_attachment_format = 0;
		pipeline_statistic_flags pipeline_statistics;
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
}
