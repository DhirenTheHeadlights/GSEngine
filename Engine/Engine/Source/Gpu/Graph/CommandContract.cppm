export module gse.gpu:command_contract;

import std;

import gse.gpu_backend;
import gse.math;
import gse.meta;

export namespace gse::gpu {
	struct command_dispatch;

	class pass_recorder {
	public:
		pass_recorder() = default;

		pass_recorder(
			command_buffer_handle cmd,
			const command_dispatch* dispatch
		);

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto native() const -> command_buffer_handle;

		auto begin() const -> void;

		auto reset() const -> void;

		auto end() const -> void;

		auto begin_rendering(
			const rendering_info& info
		) const -> void;

		auto end_rendering() const -> void;

		auto reset_query_pool(
			handle<query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count
		) const -> void;

		auto write_timestamp(
			pipeline_stage_flags stage,
			handle<query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto begin_query(
			handle<query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto end_query(
			handle<query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto release_swapchain_image_to_present(
			handle<image> img,
			pipeline_stage_flags src_stages,
			access_flags src_access
		) const -> void;

		auto set_viewport(
			const viewport& viewport
		) const -> void;

		auto set_scissor(
			const rect_t<vec2i>& scissor
		) const -> void;

		auto set_vertex_input_none() const -> void;

		auto set_topology(
			topology t
		) const -> void;

		auto set_polygon_mode(
			polygon_mode m
		) const -> void;

		auto set_cull_mode(
			cull_mode m
		) const -> void;

		auto set_front_face(
			front_face f
		) const -> void;

		auto set_depth_test_enable(
			bool enable
		) const -> void;

		auto set_depth_write_enable(
			bool enable
		) const -> void;

		auto set_depth_compare_op(
			compare_op op
		) const -> void;

		auto set_depth_bias_enable(
			bool enable
		) const -> void;

		auto set_depth_bias(
			float constant,
			float clamp,
			float slope
		) const -> void;

		auto set_depth_clamp_enable(
			bool enable
		) const -> void;

		auto set_depth_bounds_test_enable(
			bool enable
		) const -> void;

		auto set_stencil_test_enable(
			bool enable
		) const -> void;

		auto set_line_width(
			float width
		) const -> void;

		auto set_rasterizer_discard_enable(
			bool enable
		) const -> void;

		auto set_primitive_restart_enable(
			bool enable
		) const -> void;

		auto set_rasterization_samples(
			sample_count samples
		) const -> void;

		auto set_sample_mask(
			sample_count samples,
			std::uint32_t mask
		) const -> void;

		auto set_alpha_to_coverage_enable(
			bool enable
		) const -> void;

		auto set_alpha_to_one_enable(
			bool enable
		) const -> void;

		auto set_logic_op_enable(
			bool enable
		) const -> void;

		auto set_color_blend_enable(
			std::uint32_t first_attachment,
			std::span<const std::uint8_t> enables
		) const -> void;

		auto set_color_blend_equation(
			std::uint32_t first_attachment,
			std::span<const color_blend_equation> equations
		) const -> void;

		auto set_color_write_mask(
			std::uint32_t first_attachment,
			std::span<const color_component_flags> masks
		) const -> void;

		auto set_blend_constants(
			std::array<float, 4> constants
		) const -> void;

		auto bind_shaders(
			std::span<const stage_flag> stages,
			std::span<const handle<shader_object>> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const stage_flag> stages
		) const -> void;

		auto bind_index_buffer_2(
			handle<buffer> buffer,
			device_size offset,
			device_size size,
			index_type type
		) const -> void;

		auto bind_resource_heap(
			device_address heap_address,
			device_size heap_size,
			device_size reserved_offset,
			device_size reserved_size
		) const -> void;

		auto bind_sampler_heap(
			device_address heap_address,
			device_size heap_size,
			device_size reserved_offset,
			device_size reserved_size
		) const -> void;

		auto push_data(
			std::uint32_t offset,
			std::span<const std::byte> data
		) const -> void;

		auto draw(
			std::uint32_t vertex_count,
			std::uint32_t instance_count,
			std::uint32_t first_vertex,
			std::uint32_t first_instance
		) const -> void;

		auto draw_indexed(
			std::uint32_t index_count,
			std::uint32_t instance_count,
			std::uint32_t first_index,
			std::int32_t vertex_offset,
			std::uint32_t first_instance
		) const -> void;

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto draw_indexed_indirect(
			handle<buffer> buffer,
			device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks_indirect(
			handle<buffer> buffer,
			device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto dispatch_indirect(
			handle<buffer> buffer,
			device_size offset
		) const -> void;

		auto copy_buffer(
			handle<buffer> src,
			handle<buffer> dst,
			const buffer_copy_region& region
		) const -> void;

		auto fill_buffer(
			handle<buffer> dst,
			device_size offset,
			device_size size,
			std::uint32_t data
		) const -> void;

		auto copy_buffer_to_image(
			handle<buffer> src,
			handle<image> dst,
			std::span<const buffer_image_copy_region> regions
		) const -> void;

		auto copy_image_to_buffer(
			handle<image> src,
			handle<buffer> dst,
			std::span<const buffer_image_copy_region> regions
		) const -> void;

		auto blit_image(
			handle<image> src,
			handle<image> dst,
			const image_blit_region& region,
			sampler_filter filter
		) const -> void;

		auto copy_image(
			handle<image> src,
			handle<image> dst,
			const image_copy_region& region
		) const -> void;

		auto pipeline_barrier(
			const dependency_info& dep
		) const -> void;

		auto transition_image_state(
			const image_barrier& barrier
		) const -> void;

		auto build_acceleration_structures(
			const acceleration_structure_build_geometry_info& build_info,
			std::span<const acceleration_structure_build_range_info* const> range_infos
		) const -> void;

	private:
		command_buffer_handle m_cmd{};
		const command_dispatch* m_vt = nullptr;
	};
}

namespace gse::gpu {
	constexpr std::array<std::string_view, 2> command_dispatch_excludes{ "native", "valid" };

	consteval {
		std::meta::define_aggregate(
			^^command_dispatch,
			meta::dispatch_specs(std::meta::dealias(^^command_buffer_handle), ^^pass_recorder, command_dispatch_excludes)
		);
	}
}