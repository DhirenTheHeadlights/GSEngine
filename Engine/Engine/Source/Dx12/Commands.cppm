export module gse.dx12:commands;

import std;

import gse.gpu_backend;
import gse.math;
import gse.directx;

export namespace gse::dx12 {
	class commands {
	public:
		commands() = default;

		explicit commands(
			gpu::command_buffer_handle cmd
		);

		[[nodiscard]] auto native() const -> gpu::command_buffer_handle;

		[[nodiscard]] auto valid() const -> bool;

		auto begin() const -> void;

		auto begin_secondary(
			const gpu::secondary_inheritance_info& info
		) const -> void;

		auto end() const -> void;

		auto reset() const -> void;

		auto execute_commands(
			gpu::command_buffer_handle secondary
		) const -> void;

		auto begin_rendering(
			const gpu::rendering_info& info
		) const -> void;

		auto end_rendering() const -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) const -> void;

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

		auto bind_shaders(
			std::span<const gpu::stage_flag> stages,
			std::span<const gpu::handle<gpu::shader_object>> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const gpu::stage_flag> stages
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

		auto push_constants(
			gpu::handle<gpu::pipeline_layout> layout,
			gpu::stage_flags stages,
			std::uint32_t offset,
			std::uint32_t size,
			const void* data
		) const -> void;

		auto bind_index_buffer_2(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			gpu::device_size size,
			gpu::index_type type
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
			std::uint32_t group_count_x,
			std::uint32_t group_count_y,
			std::uint32_t group_count_z
		) const -> void;

		auto dispatch_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset
		) const -> void;

		auto set_viewport(
			const gpu::viewport& viewport
		) const -> void;

		auto set_scissor(
			const gse::rect_t<vec2i>& scissor
		) const -> void;

		auto set_topology(
			gpu::topology topology
		) const -> void;

		auto set_polygon_mode(
			gpu::polygon_mode mode
		) const -> void;

		auto set_cull_mode(
			gpu::cull_mode mode
		) const -> void;

		auto set_front_face(
			gpu::front_face front_face
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

		auto release_swapchain_image_to_present(
			gpu::handle<gpu::image> img,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
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
			std::uint32_t group_count_x,
			std::uint32_t group_count_y,
			std::uint32_t group_count_z
		) const -> void;

		auto build_acceleration_structures(
			const gpu::acceleration_structure_build_geometry_info& geometry_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) const -> void;

	private:
		gpu::command_buffer_handle m_cmd{};
	};
}

gse::dx12::commands::commands(const gpu::command_buffer_handle cmd) : m_cmd(cmd) {}

auto gse::dx12::commands::native() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::dx12::commands::valid() const -> bool {
	return m_cmd.value != 0;
}

auto gse::dx12::commands::begin() const -> void {}

auto gse::dx12::commands::begin_secondary(const gpu::secondary_inheritance_info&) const -> void {}

auto gse::dx12::commands::end() const -> void {}

auto gse::dx12::commands::reset() const -> void {}

auto gse::dx12::commands::execute_commands(gpu::command_buffer_handle) const -> void {}

auto gse::dx12::commands::begin_rendering(const gpu::rendering_info& info) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	std::vector<directx::D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
	rtvs.reserve(info.color_attachments.size());
	for (const auto& att : info.color_attachments) {
		const directx::D3D12_CPU_DESCRIPTOR_HANDLE rtv = {
			.ptr = std::bit_cast<std::size_t>(att.image_view),
		};
		rtvs.push_back(rtv);
		if (att.load == gpu::load_op::clear) {
			const std::array<float, 4> color = { att.color_clear_value.r, att.color_clear_value.g, att.color_clear_value.b, att.color_clear_value.a };
			list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
		}
	}
	list->OMSetRenderTargets(static_cast<std::uint32_t>(rtvs.size()), rtvs.data(), false, nullptr);
}

auto gse::dx12::commands::end_rendering() const -> void {}

auto gse::dx12::commands::pipeline_barrier(const gpu::dependency_info&) const -> void {}

auto gse::dx12::commands::reset_query_pool(gpu::handle<gpu::query_pool>, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::write_timestamp(gpu::pipeline_stage_flags, gpu::handle<gpu::query_pool>, std::uint32_t) const -> void {}

auto gse::dx12::commands::begin_query(gpu::handle<gpu::query_pool>, std::uint32_t) const -> void {}

auto gse::dx12::commands::end_query(gpu::handle<gpu::query_pool>, std::uint32_t) const -> void {}

auto gse::dx12::commands::bind_shaders(std::span<const gpu::stage_flag>, std::span<const gpu::handle<gpu::shader_object>>) const -> void {}

auto gse::dx12::commands::unbind_shaders(std::span<const gpu::stage_flag>) const -> void {}

auto gse::dx12::commands::bind_resource_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {}

auto gse::dx12::commands::bind_sampler_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {}

auto gse::dx12::commands::push_data(std::uint32_t, std::span<const std::byte>) const -> void {}

auto gse::dx12::commands::push_constants(gpu::handle<gpu::pipeline_layout>, gpu::stage_flags, std::uint32_t, std::uint32_t, const void*) const -> void {}

auto gse::dx12::commands::bind_index_buffer_2(gpu::handle<gpu::buffer>, gpu::device_size, gpu::device_size, gpu::index_type) const -> void {}

auto gse::dx12::commands::draw_indexed_indirect(gpu::handle<gpu::buffer>, gpu::device_size, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::draw_mesh_tasks_indirect(gpu::handle<gpu::buffer>, gpu::device_size, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::dispatch(std::uint32_t, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::dispatch_indirect(gpu::handle<gpu::buffer>, gpu::device_size) const -> void {}

auto gse::dx12::commands::set_viewport(const gpu::viewport&) const -> void {}

auto gse::dx12::commands::set_scissor(const gse::rect_t<vec2i>&) const -> void {}

auto gse::dx12::commands::set_topology(gpu::topology) const -> void {}

auto gse::dx12::commands::set_polygon_mode(gpu::polygon_mode) const -> void {}

auto gse::dx12::commands::set_cull_mode(gpu::cull_mode) const -> void {}

auto gse::dx12::commands::set_front_face(gpu::front_face) const -> void {}

auto gse::dx12::commands::set_depth_test_enable(bool) const -> void {}

auto gse::dx12::commands::set_depth_write_enable(bool) const -> void {}

auto gse::dx12::commands::set_depth_compare_op(gpu::compare_op) const -> void {}

auto gse::dx12::commands::set_depth_bias_enable(bool) const -> void {}

auto gse::dx12::commands::set_depth_bias(float, float, float) const -> void {}

auto gse::dx12::commands::set_depth_clamp_enable(bool) const -> void {}

auto gse::dx12::commands::set_depth_bounds_test_enable(bool) const -> void {}

auto gse::dx12::commands::set_stencil_test_enable(bool) const -> void {}

auto gse::dx12::commands::set_line_width(float) const -> void {}

auto gse::dx12::commands::set_rasterizer_discard_enable(bool) const -> void {}

auto gse::dx12::commands::set_primitive_restart_enable(bool) const -> void {}

auto gse::dx12::commands::set_rasterization_samples(gpu::sample_count) const -> void {}

auto gse::dx12::commands::set_sample_mask(gpu::sample_count, std::uint32_t) const -> void {}

auto gse::dx12::commands::set_alpha_to_coverage_enable(bool) const -> void {}

auto gse::dx12::commands::set_alpha_to_one_enable(bool) const -> void {}

auto gse::dx12::commands::set_logic_op_enable(bool) const -> void {}

auto gse::dx12::commands::set_color_blend_enable(std::uint32_t, std::span<const std::uint8_t>) const -> void {}

auto gse::dx12::commands::set_color_blend_equation(std::uint32_t, std::span<const gpu::color_blend_equation>) const -> void {}

auto gse::dx12::commands::set_color_write_mask(std::uint32_t, std::span<const gpu::color_component_flags>) const -> void {}

auto gse::dx12::commands::set_blend_constants(std::array<float, 4>) const -> void {}

auto gse::dx12::commands::copy_buffer(gpu::handle<gpu::buffer>, gpu::handle<gpu::buffer>, const gpu::buffer_copy_region&) const -> void {}

auto gse::dx12::commands::fill_buffer(gpu::handle<gpu::buffer>, gpu::device_size, gpu::device_size, std::uint32_t) const -> void {}

auto gse::dx12::commands::copy_buffer_to_image(gpu::handle<gpu::buffer>, gpu::handle<gpu::image>, std::span<const gpu::buffer_image_copy_region>) const -> void {}

auto gse::dx12::commands::copy_image_to_buffer(gpu::handle<gpu::image>, gpu::handle<gpu::buffer>, std::span<const gpu::buffer_image_copy_region>) const -> void {}

auto gse::dx12::commands::blit_image(gpu::handle<gpu::image>, gpu::handle<gpu::image>, const gpu::image_blit_region&, gpu::sampler_filter) const -> void {}

auto gse::dx12::commands::copy_image(gpu::handle<gpu::image>, gpu::handle<gpu::image>, const gpu::image_copy_region&) const -> void {}

auto gse::dx12::commands::release_swapchain_image_to_present(gpu::handle<gpu::image>, gpu::pipeline_stage_flags, gpu::access_flags) const -> void {}

auto gse::dx12::commands::draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::draw_indexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::draw_mesh_tasks(std::uint32_t, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info&, std::span<const gpu::acceleration_structure_build_range_info* const>) const -> void {}
