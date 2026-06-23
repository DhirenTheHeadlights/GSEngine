export module gse.gpu:pass_recorder;

import std;

import gse.gpu_backend;
import gse.math;

export namespace gse::gpu {
	class pass_recorder {
	public:
		pass_recorder() = default;

		pass_recorder(
			gpu::command_buffer_handle cmd
		);

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto native() const -> gpu::command_buffer_handle;

		auto begin() const -> void;

		auto reset() const -> void;

		auto end() const -> void;

		auto begin_rendering(
			const gpu::rendering_info& info
		) const -> void;

		auto end_rendering() const -> void;

		auto reset_query_pool(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count
		) const -> void;

		auto write_timestamp(
			gpu::pipeline_stage_flags stage,
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto begin_query(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto end_query(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto release_swapchain_image_to_present(
			gpu::handle<gpu::image> img,
			gpu::pipeline_stage_flags src_stages,
			gpu::access_flags src_access
		) const -> void;

		auto set_viewport(
			const gpu::viewport& viewport
		) const -> void;

		auto set_scissor(
			const gse::rect_t<vec2i>& scissor
		) const -> void;

		auto set_topology(
			gpu::topology t
		) const -> void;

		auto set_polygon_mode(
			gpu::polygon_mode m
		) const -> void;

		auto set_cull_mode(
			gpu::cull_mode m
		) const -> void;

		auto set_front_face(
			gpu::front_face f
		) const -> void;

		auto set_depth_test_enable(
			bool enable
		) const -> void;

		auto set_depth_write_enable(
			bool enable
		) const -> void;

		auto set_depth_compare_op(
			gpu::compare_op op
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
			gpu::sample_count samples
		) const -> void;

		auto set_sample_mask(
			gpu::sample_count samples,
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
			std::span<const gpu::color_blend_equation> equations
		) const -> void;

		auto set_color_write_mask(
			std::uint32_t first_attachment,
			std::span<const gpu::color_component_flags> masks
		) const -> void;

		auto set_blend_constants(
			std::array<float, 4> constants
		) const -> void;

		auto bind_shaders(
			std::span<const gpu::stage_flag> stages,
			std::span<const gpu::handle<gpu::shader_object>> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const gpu::stage_flag> stages
		) const -> void;

		auto bind_index_buffer_2(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			gpu::device_size size,
			gpu::index_type type
		) const -> void;

		auto bind_resource_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
		) const -> void;

		auto bind_sampler_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
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
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto dispatch_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset
		) const -> void;

		auto copy_buffer(
			gpu::handle<gpu::buffer> src,
			gpu::handle<gpu::buffer> dst,
			const gpu::buffer_copy_region& region
		) const -> void;

		auto fill_buffer(
			gpu::handle<gpu::buffer> dst,
			gpu::device_size offset,
			gpu::device_size size,
			std::uint32_t data
		) const -> void;

		auto copy_buffer_to_image(
			gpu::handle<gpu::buffer> src,
			gpu::handle<gpu::image> dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto copy_image_to_buffer(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::buffer> dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto blit_image(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::image> dst,
			const gpu::image_blit_region& region,
			gpu::sampler_filter filter
		) const -> void;

		auto copy_image(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::image> dst,
			const gpu::image_copy_region& region
		) const -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) const -> void;

		auto build_acceleration_structures(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) const -> void;

	private:
		gpu::command_buffer_handle m_cmd{};
	};
}
