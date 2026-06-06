export module gse.gpu:pass_recorder;

import std;

import :aliases;

import gse.meta;
import gse.vulkan;
import gse.math;

export namespace gse::gpu {
	class pass_recorder {
	public:
		pass_recorder() = default;

		pass_recorder(
			vulkan::commands cmd
		);

		[[nodiscard]] auto valid() const -> bool;

		auto end() const -> void;

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
			std::span<const gpu::shader_object_handle> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const gpu::stage_flag> stages
		) const -> void;

		auto bind_index_buffer_2(
			gpu::buffer_handle buffer,
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
			gpu::buffer_handle buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks_indirect(
			gpu::buffer_handle buffer,
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
			gpu::buffer_handle buffer,
			gpu::device_size offset
		) const -> void;

		auto copy_buffer(
			gpu::buffer_handle src,
			gpu::buffer_handle dst,
			const gpu::buffer_copy_region& region
		) const -> void;

		auto fill_buffer(
			gpu::buffer_handle dst,
			gpu::device_size offset,
			gpu::device_size size,
			std::uint32_t data
		) const -> void;

		auto copy_buffer_to_image(
			gpu::buffer_handle src,
			gpu::image_handle dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto copy_image_to_buffer(
			gpu::image_handle src,
			gpu::buffer_handle dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto blit_image(
			gpu::image_handle src,
			gpu::image_handle dst,
			const gpu::image_blit_region& region,
			gpu::sampler_filter filter
		) const -> void;

		auto copy_image(
			gpu::image_handle src,
			gpu::image_handle dst,
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
		template <gse::fixed_string Name, typename T, typename... Args>
		static auto invoke_named(T& obj, Args&&... args) -> decltype(auto) {
			template for (constexpr auto m : std::define_static_array(std::meta::members_of(^^T, std::meta::access_context::unchecked()))) {
				if constexpr (std::meta::is_function(m) && std::meta::has_identifier(m) && std::meta::identifier_of(m) == std::string_view(Name)) {
					return obj.[:m:](std::forward<Args>(args)...);
				}
			}
			std::unreachable();
		}

		template <gse::fixed_string Name, typename... Args>
		auto forward(Args&&... args) const -> decltype(auto) {
			return invoke_named<Name>(std::get<vulkan::commands>(m_impl), std::forward<Args>(args)...);
		}

		std::variant<vulkan::commands> m_impl;
	};
}

gse::gpu::pass_recorder::pass_recorder(const vulkan::commands cmd)
	: m_impl(cmd) {
}

auto gse::gpu::pass_recorder::valid() const -> bool {
	return forward<"valid">();
}

auto gse::gpu::pass_recorder::end() const -> void {
	forward<"end">();
}

auto gse::gpu::pass_recorder::set_viewport(const gpu::viewport& viewport) const -> void {
	forward<"set_viewport">(viewport);
}

auto gse::gpu::pass_recorder::set_scissor(const gse::rect_t<vec2i>& scissor) const -> void {
	forward<"set_scissor">(scissor);
}

auto gse::gpu::pass_recorder::set_topology(const gpu::topology t) const -> void {
	forward<"set_topology">(t);
}

auto gse::gpu::pass_recorder::set_polygon_mode(const gpu::polygon_mode m) const -> void {
	forward<"set_polygon_mode">(m);
}

auto gse::gpu::pass_recorder::set_cull_mode(const gpu::cull_mode m) const -> void {
	forward<"set_cull_mode">(m);
}

auto gse::gpu::pass_recorder::set_front_face(const gpu::front_face f) const -> void {
	forward<"set_front_face">(f);
}

auto gse::gpu::pass_recorder::set_depth_test_enable(const bool enable) const -> void {
	forward<"set_depth_test_enable">(enable);
}

auto gse::gpu::pass_recorder::set_depth_write_enable(const bool enable) const -> void {
	forward<"set_depth_write_enable">(enable);
}

auto gse::gpu::pass_recorder::set_depth_compare_op(const gpu::compare_op op) const -> void {
	forward<"set_depth_compare_op">(op);
}

auto gse::gpu::pass_recorder::set_depth_bias_enable(const bool enable) const -> void {
	forward<"set_depth_bias_enable">(enable);
}

auto gse::gpu::pass_recorder::set_depth_bias(const float constant, const float clamp, const float slope) const -> void {
	forward<"set_depth_bias">(constant, clamp, slope);
}

auto gse::gpu::pass_recorder::set_depth_clamp_enable(const bool enable) const -> void {
	forward<"set_depth_clamp_enable">(enable);
}

auto gse::gpu::pass_recorder::set_depth_bounds_test_enable(const bool enable) const -> void {
	forward<"set_depth_bounds_test_enable">(enable);
}

auto gse::gpu::pass_recorder::set_stencil_test_enable(const bool enable) const -> void {
	forward<"set_stencil_test_enable">(enable);
}

auto gse::gpu::pass_recorder::set_line_width(const float width) const -> void {
	forward<"set_line_width">(width);
}

auto gse::gpu::pass_recorder::set_rasterizer_discard_enable(const bool enable) const -> void {
	forward<"set_rasterizer_discard_enable">(enable);
}

auto gse::gpu::pass_recorder::set_primitive_restart_enable(const bool enable) const -> void {
	forward<"set_primitive_restart_enable">(enable);
}

auto gse::gpu::pass_recorder::set_rasterization_samples(const gpu::sample_count samples) const -> void {
	forward<"set_rasterization_samples">(samples);
}

auto gse::gpu::pass_recorder::set_sample_mask(const gpu::sample_count samples, const std::uint32_t mask) const -> void {
	forward<"set_sample_mask">(samples, mask);
}

auto gse::gpu::pass_recorder::set_alpha_to_coverage_enable(const bool enable) const -> void {
	forward<"set_alpha_to_coverage_enable">(enable);
}

auto gse::gpu::pass_recorder::set_alpha_to_one_enable(const bool enable) const -> void {
	forward<"set_alpha_to_one_enable">(enable);
}

auto gse::gpu::pass_recorder::set_logic_op_enable(const bool enable) const -> void {
	forward<"set_logic_op_enable">(enable);
}

auto gse::gpu::pass_recorder::set_color_blend_enable(const std::uint32_t first_attachment, const std::span<const std::uint8_t> enables) const -> void {
	forward<"set_color_blend_enable">(first_attachment, enables);
}

auto gse::gpu::pass_recorder::set_color_blend_equation(const std::uint32_t first_attachment, const std::span<const gpu::color_blend_equation> equations) const -> void {
	forward<"set_color_blend_equation">(first_attachment, equations);
}

auto gse::gpu::pass_recorder::set_color_write_mask(const std::uint32_t first_attachment, const std::span<const gpu::color_component_flags> masks) const -> void {
	forward<"set_color_write_mask">(first_attachment, masks);
}

auto gse::gpu::pass_recorder::set_blend_constants(const std::array<float, 4> constants) const -> void {
	forward<"set_blend_constants">(constants);
}

auto gse::gpu::pass_recorder::bind_shaders(const std::span<const gpu::stage_flag> stages, const std::span<const gpu::shader_object_handle> shaders) const -> void {
	forward<"bind_shaders">(stages, shaders);
}

auto gse::gpu::pass_recorder::unbind_shaders(const std::span<const gpu::stage_flag> stages) const -> void {
	forward<"unbind_shaders">(stages);
}

auto gse::gpu::pass_recorder::bind_index_buffer_2(const gpu::buffer_handle buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) const -> void {
	forward<"bind_index_buffer_2">(buffer, offset, size, type);
}

auto gse::gpu::pass_recorder::bind_resource_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	forward<"bind_resource_heap">(heap_address, heap_size, reserved_offset, reserved_size);
}

auto gse::gpu::pass_recorder::bind_sampler_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	forward<"bind_sampler_heap">(heap_address, heap_size, reserved_offset, reserved_size);
}

auto gse::gpu::pass_recorder::push_data(const std::uint32_t offset, const std::span<const std::byte> data) const -> void {
	forward<"push_data">(offset, data);
}

auto gse::gpu::pass_recorder::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	forward<"draw">(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::gpu::pass_recorder::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	forward<"draw_indexed">(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::gpu::pass_recorder::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	forward<"draw_mesh_tasks">(x, y, z);
}

auto gse::gpu::pass_recorder::draw_indexed_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	forward<"draw_indexed_indirect">(buffer, offset, draw_count, stride);
}

auto gse::gpu::pass_recorder::draw_mesh_tasks_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	forward<"draw_mesh_tasks_indirect">(buffer, offset, draw_count, stride);
}

auto gse::gpu::pass_recorder::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	forward<"dispatch">(x, y, z);
}

auto gse::gpu::pass_recorder::dispatch_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset) const -> void {
	forward<"dispatch_indirect">(buffer, offset);
}

auto gse::gpu::pass_recorder::copy_buffer(const gpu::buffer_handle src, const gpu::buffer_handle dst, const gpu::buffer_copy_region& region) const -> void {
	forward<"copy_buffer">(src, dst, region);
}

auto gse::gpu::pass_recorder::fill_buffer(const gpu::buffer_handle dst, const gpu::device_size offset, const gpu::device_size size, const std::uint32_t data) const -> void {
	forward<"fill_buffer">(dst, offset, size, data);
}

auto gse::gpu::pass_recorder::copy_buffer_to_image(const gpu::buffer_handle src, const gpu::image_handle dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	forward<"copy_buffer_to_image">(src, dst, regions);
}

auto gse::gpu::pass_recorder::copy_image_to_buffer(const gpu::image_handle src, const gpu::buffer_handle dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	forward<"copy_image_to_buffer">(src, dst, regions);
}

auto gse::gpu::pass_recorder::blit_image(const gpu::image_handle src, const gpu::image_handle dst, const gpu::image_blit_region& region, const gpu::sampler_filter filter) const -> void {
	forward<"blit_image">(src, dst, region, filter);
}

auto gse::gpu::pass_recorder::copy_image(const gpu::image_handle src, const gpu::image_handle dst, const gpu::image_copy_region& region) const -> void {
	forward<"copy_image">(src, dst, region);
}

auto gse::gpu::pass_recorder::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	forward<"pipeline_barrier">(dep);
}

auto gse::gpu::pass_recorder::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info& build_info, const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos) const -> void {
	forward<"build_acceleration_structures">(build_info, range_infos);
}
