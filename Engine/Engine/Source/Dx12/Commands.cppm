export module gse.dx12:commands;

import std;

import gse.gpu_backend;
import gse.math;
import gse.directx;

import :device;

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

		auto end() const -> void;

		auto reset() const -> void;

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

		auto set_vertex_input_none() const -> void;

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

auto gse::dx12::commands::end() const -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd)) {
		list->Close();
	}
}

auto gse::dx12::commands::reset() const -> void {}

auto gse::dx12::commands::begin_rendering(const gpu::rendering_info& info) const -> void {
	if (active_device) {
		active_device->cmd_begin_rendering(m_cmd, info);
	}
}

auto gse::dx12::commands::end_rendering() const -> void {}

auto gse::dx12::commands::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	if (active_device) {
		active_device->cmd_pipeline_barrier(m_cmd, dep);
	}
}

auto gse::dx12::commands::reset_query_pool(gpu::handle<gpu::query_pool>, std::uint32_t, std::uint32_t) const -> void {}

auto gse::dx12::commands::write_timestamp(gpu::pipeline_stage_flags, const gpu::handle<gpu::query_pool> pool, const std::uint32_t index) const -> void {
	if (active_device) {
		active_device->cmd_write_timestamp(m_cmd, pool, index);
	}
}

auto gse::dx12::commands::begin_query(gpu::handle<gpu::query_pool>, std::uint32_t) const -> void {}

auto gse::dx12::commands::end_query(gpu::handle<gpu::query_pool>, std::uint32_t) const -> void {}

auto gse::dx12::commands::bind_shaders(const std::span<const gpu::stage_flag> stages, const std::span<const gpu::handle<gpu::shader_object>> shaders) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	if (!list || shaders.empty()) {
		return;
	}
	const bool is_compute = std::ranges::find(stages, gpu::stage_flag::compute) != stages.end();
	if (is_compute) {
		if (auto* pso = std::bit_cast<directx::ID3D12PipelineState*>(shaders[0])) {
			list->SetPipelineState(pso);
			if (active_device) {
				active_device->note_compute_push_size(m_cmd, shaders[0]);
			}
		}
		return;
	}
	for (const auto h : shaders) {
		if (h && active_device) {
			active_device->cmd_bind_graphics_shaders(m_cmd, h);
			return;
		}
	}
}

auto gse::dx12::commands::unbind_shaders(std::span<const gpu::stage_flag>) const -> void {}

auto gse::dx12::commands::bind_resource_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	if (!list || !active_device) {
		return;
	}
	directx::ID3D12DescriptorHeap* heaps[]{ active_device->resource_heap(), active_device->sampler_heap() };
	list->SetDescriptorHeaps(2, heaps);
	auto* root = active_device->root_signature();
	list->SetComputeRootSignature(root);
	if (!directx::is_compute_command_list(list)) {
		list->SetGraphicsRootSignature(root);
	}
}

auto gse::dx12::commands::bind_sampler_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {}

auto gse::dx12::commands::push_data(const std::uint32_t offset, const std::span<const std::byte> data) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	if (!list || data.empty()) {
		return;
	}
	const auto num_values = static_cast<std::uint32_t>(data.size() / 4);
	const auto push_size = active_device ? active_device->list_push_size(m_cmd) : 0;
	const bool compute_list = directx::is_compute_command_list(list);
	if (offset < push_size) {
		const auto dest_offset = offset / 4;
		list->SetComputeRoot32BitConstants(1, num_values, data.data(), dest_offset);
		if (!compute_list) {
			list->SetGraphicsRoot32BitConstants(1, num_values, data.data(), dest_offset);
		}
	}
	else {
		const auto dest_offset = (offset - push_size) / 4;
		list->SetComputeRoot32BitConstants(0, num_values, data.data(), dest_offset);
		if (!compute_list) {
			list->SetGraphicsRoot32BitConstants(0, num_values, data.data(), dest_offset);
		}
	}
}

auto gse::dx12::commands::push_constants(gpu::handle<gpu::pipeline_layout>, gpu::stage_flags, std::uint32_t, std::uint32_t, const void*) const -> void {}

auto gse::dx12::commands::bind_index_buffer_2(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) const -> void {
	if (active_device) {
		active_device->cmd_bind_index_buffer(m_cmd, buffer, offset, size, type);
	}
}

auto gse::dx12::commands::draw_indexed_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	if (active_device) {
		active_device->cmd_draw_indexed_indirect(m_cmd, buffer, offset, draw_count, stride);
	}
}

auto gse::dx12::commands::draw_mesh_tasks_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	if (active_device) {
		active_device->cmd_draw_mesh_tasks_indirect(m_cmd, buffer, offset, draw_count, stride);
	}
}

auto gse::dx12::commands::dispatch(const std::uint32_t group_count_x, const std::uint32_t group_count_y, const std::uint32_t group_count_z) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	if (!list) {
		return;
	}
	if (active_device && !active_device->compute_pso_bound(m_cmd)) {
		return;
	}
	list->Dispatch(group_count_x, group_count_y, group_count_z);
}

auto gse::dx12::commands::dispatch_indirect(gpu::handle<gpu::buffer>, gpu::device_size) const -> void {}

auto gse::dx12::commands::set_viewport(const gpu::viewport& viewport) const -> void {
	if (active_device) {
		active_device->cmd_set_viewport(m_cmd, viewport);
	}
}

auto gse::dx12::commands::set_scissor(const gse::rect_t<vec2i>& scissor) const -> void {
	if (active_device) {
		active_device->cmd_set_scissor(m_cmd, scissor);
	}
}

auto gse::dx12::commands::set_vertex_input_none() const -> void {}

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

auto gse::dx12::commands::copy_buffer(const gpu::handle<gpu::buffer> src, const gpu::handle<gpu::buffer> dst, const gpu::buffer_copy_region& region) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	auto* src_res = std::bit_cast<directx::ID3D12Resource*>(src);
	auto* dst_res = std::bit_cast<directx::ID3D12Resource*>(dst);
	if (list && src_res && dst_res) {
		list->CopyBufferRegion(dst_res, region.dst_offset, src_res, region.src_offset, region.size);
	}
}

auto gse::dx12::commands::fill_buffer(gpu::handle<gpu::buffer>, gpu::device_size, gpu::device_size, std::uint32_t) const -> void {}

auto gse::dx12::commands::copy_buffer_to_image(gpu::handle<gpu::buffer>, gpu::handle<gpu::image>, std::span<const gpu::buffer_image_copy_region>) const -> void {}

auto gse::dx12::commands::copy_image_to_buffer(gpu::handle<gpu::image>, gpu::handle<gpu::buffer>, std::span<const gpu::buffer_image_copy_region>) const -> void {}

auto gse::dx12::commands::blit_image(gpu::handle<gpu::image>, gpu::handle<gpu::image>, const gpu::image_blit_region&, gpu::sampler_filter) const -> void {}

auto gse::dx12::commands::copy_image(gpu::handle<gpu::image>, gpu::handle<gpu::image>, const gpu::image_copy_region&) const -> void {}

auto gse::dx12::commands::release_swapchain_image_to_present(gpu::handle<gpu::image>, gpu::pipeline_stage_flags, gpu::access_flags) const -> void {}

auto gse::dx12::commands::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	if (active_device) {
		active_device->cmd_draw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
	}
}

auto gse::dx12::commands::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	if (active_device) {
		active_device->cmd_draw_indexed(m_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
	}
}

auto gse::dx12::commands::draw_mesh_tasks(const std::uint32_t group_count_x, const std::uint32_t group_count_y, const std::uint32_t group_count_z) const -> void {
	if (active_device) {
		active_device->cmd_draw_mesh_tasks(m_cmd, group_count_x, group_count_y, group_count_z);
	}
}

auto gse::dx12::commands::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info& geometry_info, const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos) const -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(m_cmd);
	if (!list || geometry_info.geometries.empty() || range_infos.empty()) {
		return;
	}
	const auto& geometry = geometry_info.geometries[0];
	const auto dst = geometry_info.dst.value;
	const auto scratch = geometry_info.scratch_address;
	if (geometry_info.type == gpu::acceleration_structure_type::bottom_level) {
		const directx::blas_triangles triangles{
			.vertex_format = directx::format_r32g32b32_float,
			.vertex_address = geometry.triangles.vertex_data,
			.vertex_stride = geometry.triangles.vertex_stride,
			.vertex_count = geometry.triangles.max_vertex + 1,
			.index_format = directx::format_r32_uint,
			.index_address = geometry.triangles.index_data,
			.prim_count = range_infos[0]->primitive_count,
		};
		directx::build_blas(list, dst, scratch, triangles);
	}
	else {
		const bool allow_update = geometry_info.flags.test(gpu::build_acceleration_structure_flag::allow_update);
		directx::build_tlas(list, dst, scratch, geometry.instances.data, range_infos[0]->primitive_count, allow_update);
	}
}
