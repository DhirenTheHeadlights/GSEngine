module gse.gpu:pass_recorder_impl;

import std;

import :command_dispatch;

import gse.gpu_backend;
import gse.math;

gse::gpu::pass_recorder::pass_recorder(const gpu::command_buffer_handle cmd, const command_dispatch* dispatch)
	: m_cmd(cmd), m_vt(dispatch) {
}

auto gse::gpu::pass_recorder::valid() const -> bool {
	return static_cast<bool>(m_cmd);
}

auto gse::gpu::pass_recorder::native() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::gpu::pass_recorder::begin() const -> void {
	m_vt->begin(m_cmd);
}

auto gse::gpu::pass_recorder::reset() const -> void {
	m_vt->reset(m_cmd);
}

auto gse::gpu::pass_recorder::end() const -> void {
	m_vt->end(m_cmd);
}

auto gse::gpu::pass_recorder::begin_rendering(const gpu::rendering_info& info) const -> void {
	m_vt->begin_rendering(m_cmd, info);
}

auto gse::gpu::pass_recorder::end_rendering() const -> void {
	m_vt->end_rendering(m_cmd);
}

auto gse::gpu::pass_recorder::reset_query_pool(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count) const -> void {
	m_vt->reset_query_pool(m_cmd, pool, first_query, query_count);
}

auto gse::gpu::pass_recorder::write_timestamp(const gpu::pipeline_stage_flags stage, const gpu::handle<gpu::query_pool> pool, const std::uint32_t query_index) const -> void {
	m_vt->write_timestamp(m_cmd, stage, pool, query_index);
}

auto gse::gpu::pass_recorder::begin_query(const gpu::handle<gpu::query_pool> pool, const std::uint32_t query_index) const -> void {
	m_vt->begin_query(m_cmd, pool, query_index);
}

auto gse::gpu::pass_recorder::end_query(const gpu::handle<gpu::query_pool> pool, const std::uint32_t query_index) const -> void {
	m_vt->end_query(m_cmd, pool, query_index);
}

auto gse::gpu::pass_recorder::release_swapchain_image_to_present(const gpu::handle<gpu::image> img, const gpu::pipeline_stage_flags src_stages, const gpu::access_flags src_access) const -> void {
	m_vt->release_swapchain_image_to_present(m_cmd, img, src_stages, src_access);
}

auto gse::gpu::pass_recorder::set_viewport(const gpu::viewport& viewport) const -> void {
	m_vt->set_viewport(m_cmd, viewport);
}

auto gse::gpu::pass_recorder::set_scissor(const gse::rect_t<vec2i>& scissor) const -> void {
	m_vt->set_scissor(m_cmd, scissor);
}

auto gse::gpu::pass_recorder::set_vertex_input_none() const -> void {
	m_vt->set_vertex_input_none(m_cmd);
}

auto gse::gpu::pass_recorder::set_topology(const gpu::topology t) const -> void {
	m_vt->set_topology(m_cmd, t);
}

auto gse::gpu::pass_recorder::set_polygon_mode(const gpu::polygon_mode m) const -> void {
	m_vt->set_polygon_mode(m_cmd, m);
}

auto gse::gpu::pass_recorder::set_cull_mode(const gpu::cull_mode m) const -> void {
	m_vt->set_cull_mode(m_cmd, m);
}

auto gse::gpu::pass_recorder::set_front_face(const gpu::front_face f) const -> void {
	m_vt->set_front_face(m_cmd, f);
}

auto gse::gpu::pass_recorder::set_depth_test_enable(const bool enable) const -> void {
	m_vt->set_depth_test_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_depth_write_enable(const bool enable) const -> void {
	m_vt->set_depth_write_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_depth_compare_op(const gpu::compare_op op) const -> void {
	m_vt->set_depth_compare_op(m_cmd, op);
}

auto gse::gpu::pass_recorder::set_depth_bias_enable(const bool enable) const -> void {
	m_vt->set_depth_bias_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_depth_bias(const float constant, const float clamp, const float slope) const -> void {
	m_vt->set_depth_bias(m_cmd, constant, clamp, slope);
}

auto gse::gpu::pass_recorder::set_depth_clamp_enable(const bool enable) const -> void {
	m_vt->set_depth_clamp_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_depth_bounds_test_enable(const bool enable) const -> void {
	m_vt->set_depth_bounds_test_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_stencil_test_enable(const bool enable) const -> void {
	m_vt->set_stencil_test_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_line_width(const float width) const -> void {
	m_vt->set_line_width(m_cmd, width);
}

auto gse::gpu::pass_recorder::set_rasterizer_discard_enable(const bool enable) const -> void {
	m_vt->set_rasterizer_discard_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_primitive_restart_enable(const bool enable) const -> void {
	m_vt->set_primitive_restart_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_rasterization_samples(const gpu::sample_count samples) const -> void {
	m_vt->set_rasterization_samples(m_cmd, samples);
}

auto gse::gpu::pass_recorder::set_sample_mask(const gpu::sample_count samples, const std::uint32_t mask) const -> void {
	m_vt->set_sample_mask(m_cmd, samples, mask);
}

auto gse::gpu::pass_recorder::set_alpha_to_coverage_enable(const bool enable) const -> void {
	m_vt->set_alpha_to_coverage_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_alpha_to_one_enable(const bool enable) const -> void {
	m_vt->set_alpha_to_one_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_logic_op_enable(const bool enable) const -> void {
	m_vt->set_logic_op_enable(m_cmd, enable);
}

auto gse::gpu::pass_recorder::set_color_blend_enable(const std::uint32_t first_attachment, const std::span<const std::uint8_t> enables) const -> void {
	m_vt->set_color_blend_enable(m_cmd, first_attachment, enables);
}

auto gse::gpu::pass_recorder::set_color_blend_equation(const std::uint32_t first_attachment, const std::span<const gpu::color_blend_equation> equations) const -> void {
	m_vt->set_color_blend_equation(m_cmd, first_attachment, equations);
}

auto gse::gpu::pass_recorder::set_color_write_mask(const std::uint32_t first_attachment, const std::span<const gpu::color_component_flags> masks) const -> void {
	m_vt->set_color_write_mask(m_cmd, first_attachment, masks);
}

auto gse::gpu::pass_recorder::set_blend_constants(const std::array<float, 4> constants) const -> void {
	m_vt->set_blend_constants(m_cmd, constants);
}

auto gse::gpu::pass_recorder::bind_shaders(const std::span<const gpu::stage_flag> stages, const std::span<const gpu::handle<gpu::shader_object>> shaders) const -> void {
	m_vt->bind_shaders(m_cmd, stages, shaders);
}

auto gse::gpu::pass_recorder::unbind_shaders(const std::span<const gpu::stage_flag> stages) const -> void {
	m_vt->unbind_shaders(m_cmd, stages);
}

auto gse::gpu::pass_recorder::bind_index_buffer_2(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) const -> void {
	m_vt->bind_index_buffer_2(m_cmd, buffer, offset, size, type);
}

auto gse::gpu::pass_recorder::bind_resource_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	m_vt->bind_resource_heap(m_cmd, heap_address, heap_size, reserved_offset, reserved_size);
}

auto gse::gpu::pass_recorder::bind_sampler_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	m_vt->bind_sampler_heap(m_cmd, heap_address, heap_size, reserved_offset, reserved_size);
}

auto gse::gpu::pass_recorder::push_data(const std::uint32_t offset, const std::span<const std::byte> data) const -> void {
	m_vt->push_data(m_cmd, offset, data);
}

auto gse::gpu::pass_recorder::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	m_vt->draw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::gpu::pass_recorder::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	m_vt->draw_indexed(m_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::gpu::pass_recorder::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	m_vt->draw_mesh_tasks(m_cmd, x, y, z);
}

auto gse::gpu::pass_recorder::draw_indexed_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	m_vt->draw_indexed_indirect(m_cmd, buffer, offset, draw_count, stride);
}

auto gse::gpu::pass_recorder::draw_mesh_tasks_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	m_vt->draw_mesh_tasks_indirect(m_cmd, buffer, offset, draw_count, stride);
}

auto gse::gpu::pass_recorder::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	m_vt->dispatch(m_cmd, x, y, z);
}

auto gse::gpu::pass_recorder::dispatch_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset) const -> void {
	m_vt->dispatch_indirect(m_cmd, buffer, offset);
}

auto gse::gpu::pass_recorder::copy_buffer(const gpu::handle<gpu::buffer> src, const gpu::handle<gpu::buffer> dst, const gpu::buffer_copy_region& region) const -> void {
	m_vt->copy_buffer(m_cmd, src, dst, region);
}

auto gse::gpu::pass_recorder::fill_buffer(const gpu::handle<gpu::buffer> dst, const gpu::device_size offset, const gpu::device_size size, const std::uint32_t data) const -> void {
	m_vt->fill_buffer(m_cmd, dst, offset, size, data);
}

auto gse::gpu::pass_recorder::copy_buffer_to_image(const gpu::handle<gpu::buffer> src, const gpu::handle<gpu::image> dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	m_vt->copy_buffer_to_image(m_cmd, src, dst, regions);
}

auto gse::gpu::pass_recorder::copy_image_to_buffer(const gpu::handle<gpu::image> src, const gpu::handle<gpu::buffer> dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	m_vt->copy_image_to_buffer(m_cmd, src, dst, regions);
}

auto gse::gpu::pass_recorder::blit_image(const gpu::handle<gpu::image> src, const gpu::handle<gpu::image> dst, const gpu::image_blit_region& region, const gpu::sampler_filter filter) const -> void {
	m_vt->blit_image(m_cmd, src, dst, region, filter);
}

auto gse::gpu::pass_recorder::copy_image(const gpu::handle<gpu::image> src, const gpu::handle<gpu::image> dst, const gpu::image_copy_region& region) const -> void {
	m_vt->copy_image(m_cmd, src, dst, region);
}

auto gse::gpu::pass_recorder::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	m_vt->pipeline_barrier(m_cmd, dep);
}

auto gse::gpu::pass_recorder::transition_image_state(const gpu::image_barrier& barrier) const -> void {
	m_vt->transition_image_state(m_cmd, barrier);
}

auto gse::gpu::pass_recorder::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info& build_info, const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos) const -> void {
	m_vt->build_acceleration_structures(m_cmd, build_info, range_infos);
}
