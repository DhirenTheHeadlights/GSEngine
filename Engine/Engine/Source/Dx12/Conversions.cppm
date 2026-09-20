export module gse.dx12:conversions;

import gse.gpu_backend;
import gse.directx;

export namespace gse::dx12 {
	[[nodiscard]] auto dxgi_format_of(
		gpu::image_format fmt
	) -> directx::DXGI_FORMAT;

	[[nodiscard]] auto resource_format_of(
		gpu::image_format fmt
	) -> directx::DXGI_FORMAT;

	[[nodiscard]] auto srv_format_of(
		gpu::image_format fmt
	) -> directx::DXGI_FORMAT;

	[[nodiscard]] auto barrier_sync_of(
		gpu::pipeline_stage_flags stages,
		gpu::access_flags access,
		bool compute_queue
	) -> directx::D3D12_BARRIER_SYNC;

	[[nodiscard]] auto barrier_access_of(
		gpu::access_flags access,
		bool compute_queue
	) -> directx::D3D12_BARRIER_ACCESS;

	[[nodiscard]] auto primitive_topology_of(
		gpu::topology t
	) -> directx::D3D12_PRIMITIVE_TOPOLOGY;
}

auto gse::dx12::dxgi_format_of(const gpu::image_format fmt) -> directx::DXGI_FORMAT {
	switch (fmt) {
		case gpu::image_format::r8g8b8a8_unorm: return directx::format_r8g8b8a8_unorm;
		case gpu::image_format::r8g8b8a8_srgb: return directx::format_r8g8b8a8_srgb;
		case gpu::image_format::b8g8r8a8_unorm: return directx::format_b8g8r8a8_unorm;
		case gpu::image_format::b8g8r8a8_srgb: return directx::format_b8g8r8a8_srgb;
		case gpu::image_format::r8g8b8_unorm: return directx::format_r8g8b8a8_unorm;
		case gpu::image_format::r8g8b8_srgb: return directx::format_r8g8b8a8_srgb;
		case gpu::image_format::r8_unorm: return directx::format_r8_unorm;
		case gpu::image_format::r8g8_unorm: return directx::format_r8g8_unorm;
		case gpu::image_format::r8g8_snorm: return directx::format_r8g8_snorm;
		case gpu::image_format::b10g11r11_ufloat: return directx::format_r11g11b10_float;
		case gpu::image_format::r16g16b16a16_sfloat: return directx::format_r16g16b16a16_float;
		case gpu::image_format::r16g16_sfloat: return directx::format_r16g16_float;
		case gpu::image_format::d32_sfloat: return directx::format_d32_float;
		case gpu::image_format::undefined: return directx::format_unknown;
		default: return directx::format_b8g8r8a8_unorm;
	}
}

auto gse::dx12::resource_format_of(const gpu::image_format fmt) -> directx::DXGI_FORMAT {
	if (fmt == gpu::image_format::d32_sfloat) {
		return directx::format_r32_typeless;
	}
	return dxgi_format_of(fmt);
}

auto gse::dx12::srv_format_of(const gpu::image_format fmt) -> directx::DXGI_FORMAT {
	if (fmt == gpu::image_format::d32_sfloat) {
		return directx::format_r32_float;
	}
	return dxgi_format_of(fmt);
}

auto gse::dx12::barrier_sync_of(const gpu::pipeline_stage_flags stages, const gpu::access_flags access, const bool compute_queue) -> directx::D3D12_BARRIER_SYNC {
	int sync = 0;
	const auto add = [&](const directx::D3D12_BARRIER_SYNC s) {
		sync |= static_cast<int>(s);
	};
	if (stages.test(gpu::pipeline_stage_flag::all_commands) || stages.test(gpu::pipeline_stage_flag::host)) {
		return directx::sync_all;
	}
	if (stages.test(gpu::pipeline_stage_flag::draw_indirect)) {
		add(directx::sync_execute_indirect);
	}
	if (stages.test(gpu::pipeline_stage_flag::vertex_input) || stages.test(gpu::pipeline_stage_flag::index_input) || stages.test(gpu::pipeline_stage_flag::vertex_attribute_input)) {
		add(directx::sync_index_input);
	}
	if (stages.test(gpu::pipeline_stage_flag::vertex_shader) || stages.test(gpu::pipeline_stage_flag::tessellation_control) || stages.test(gpu::pipeline_stage_flag::tessellation_evaluation) || stages.test(gpu::pipeline_stage_flag::geometry_shader) || stages.test(gpu::pipeline_stage_flag::pre_rasterization_shaders) || stages.test(gpu::pipeline_stage_flag::mesh_shader) || stages.test(gpu::pipeline_stage_flag::task_shader)) {
		add(directx::sync_vertex_shading);
	}
	if (stages.test(gpu::pipeline_stage_flag::fragment_shader)) {
		add(directx::sync_pixel_shading);
	}
	if (stages.test(gpu::pipeline_stage_flag::early_fragment_tests) || stages.test(gpu::pipeline_stage_flag::late_fragment_tests)) {
		add(directx::sync_depth_stencil);
	}
	if (stages.test(gpu::pipeline_stage_flag::color_attachment_output)) {
		add(directx::sync_render_target);
	}
	if (stages.test(gpu::pipeline_stage_flag::all_graphics)) {
		add(directx::sync_draw);
	}
	if (stages.test(gpu::pipeline_stage_flag::compute_shader)) {
		add(directx::sync_compute_shading);
	}
	if (stages.test(gpu::pipeline_stage_flag::ray_tracing_shader)) {
		add(directx::sync_raytracing);
	}
	if (stages.test(gpu::pipeline_stage_flag::transfer) || stages.test(gpu::pipeline_stage_flag::copy) || stages.test(gpu::pipeline_stage_flag::blit) || stages.test(gpu::pipeline_stage_flag::clear)) {
		add(directx::sync_copy);
	}
	if (stages.test(gpu::pipeline_stage_flag::resolve)) {
		add(directx::sync_resolve);
	}
	if (stages.test(gpu::pipeline_stage_flag::acceleration_structure_build)) {
		add(directx::sync_build_raytracing_acceleration_structure);
	}

	if (access.test(gpu::access_flag::color_attachment_read) || access.test(gpu::access_flag::color_attachment_write)) {
		add(directx::sync_render_target);
	}
	if (access.test(gpu::access_flag::depth_stencil_attachment_read) || access.test(gpu::access_flag::depth_stencil_attachment_write)) {
		add(directx::sync_depth_stencil);
	}
	if (access.test(gpu::access_flag::transfer_read) || access.test(gpu::access_flag::transfer_write)) {
		add(directx::sync_copy);
	}
	if (access.test(gpu::access_flag::indirect_command_read)) {
		add(directx::sync_execute_indirect);
	}
	if (access.test(gpu::access_flag::index_read)) {
		add(directx::sync_index_input);
	}
	if (access.test(gpu::access_flag::acceleration_structure_write)) {
		add(directx::sync_build_raytracing_acceleration_structure);
	}
	const bool shader_access = access.test(gpu::access_flag::shader_read) || access.test(gpu::access_flag::shader_write) || access.test(gpu::access_flag::shader_sampled_read) || access.test(gpu::access_flag::shader_storage_read) || access.test(gpu::access_flag::shader_storage_write) || access.test(gpu::access_flag::uniform_read) || access.test(gpu::access_flag::acceleration_structure_read);
	constexpr int shading_mask = static_cast<int>(directx::sync_vertex_shading) | static_cast<int>(directx::sync_pixel_shading) | static_cast<int>(directx::sync_compute_shading) | static_cast<int>(directx::sync_raytracing) | static_cast<int>(directx::sync_build_raytracing_acceleration_structure);
	if (shader_access && (sync & shading_mask) == 0) {
		add(directx::sync_all_shading);
	}
	if (access.test(gpu::access_flag::acceleration_structure_read) || access.test(gpu::access_flag::acceleration_structure_write)) {
		constexpr int graphics_shading = static_cast<int>(directx::sync_vertex_shading) | static_cast<int>(directx::sync_pixel_shading) | static_cast<int>(directx::sync_draw);
		if ((sync & graphics_shading) != 0) {
			sync &= ~graphics_shading;
			add(directx::sync_all_shading);
		}
	}

	if (compute_queue) {
		constexpr int graphics_only = static_cast<int>(directx::sync_draw) | static_cast<int>(directx::sync_index_input) | static_cast<int>(directx::sync_vertex_shading) | static_cast<int>(directx::sync_pixel_shading) | static_cast<int>(directx::sync_depth_stencil) | static_cast<int>(directx::sync_render_target) | static_cast<int>(directx::sync_resolve);
		sync &= ~graphics_only;
	}
	if (sync == 0) {
		return directx::sync_all;
	}
	return static_cast<directx::D3D12_BARRIER_SYNC>(sync);
}

auto gse::dx12::barrier_access_of(const gpu::access_flags access, const bool compute_queue) -> directx::D3D12_BARRIER_ACCESS {
	int bits = 0;
	const auto add = [&](const directx::D3D12_BARRIER_ACCESS a) {
		bits |= static_cast<int>(a);
	};
	if (access.test(gpu::access_flag::indirect_command_read)) {
		add(directx::access_indirect_argument);
	}
	if (access.test(gpu::access_flag::index_read)) {
		add(directx::access_index_buffer);
	}
	if (access.test(gpu::access_flag::vertex_attribute_read)) {
		add(directx::access_vertex_buffer);
	}
	if (access.test(gpu::access_flag::uniform_read) || access.test(gpu::access_flag::shader_read) || access.test(gpu::access_flag::shader_sampled_read) || access.test(gpu::access_flag::input_attachment_read)) {
		add(directx::access_shader_resource);
	}
	if (access.test(gpu::access_flag::shader_storage_read)) {
		add(directx::access_shader_resource);
		add(directx::access_unordered_access);
	}
	if (access.test(gpu::access_flag::shader_write) || access.test(gpu::access_flag::shader_storage_write)) {
		add(directx::access_unordered_access);
	}
	if (access.test(gpu::access_flag::color_attachment_read) || access.test(gpu::access_flag::color_attachment_write)) {
		add(directx::access_render_target);
	}
	if (access.test(gpu::access_flag::depth_stencil_attachment_write)) {
		add(directx::access_depth_stencil_write);
	}
	if (access.test(gpu::access_flag::depth_stencil_attachment_read)) {
		add(directx::access_depth_stencil_read);
	}
	if (access.test(gpu::access_flag::transfer_read)) {
		add(directx::access_copy_source);
	}
	if (access.test(gpu::access_flag::transfer_write)) {
		add(directx::access_copy_dest);
	}
	if (access.test(gpu::access_flag::acceleration_structure_read)) {
		add(directx::access_raytracing_acceleration_structure_read);
	}
	if (access.test(gpu::access_flag::acceleration_structure_write)) {
		add(directx::access_raytracing_acceleration_structure_write);
	}
	if (compute_queue) {
		constexpr int graphics_only = static_cast<int>(directx::access_render_target) | static_cast<int>(directx::access_depth_stencil_write) | static_cast<int>(directx::access_depth_stencil_read) | static_cast<int>(directx::access_index_buffer) | static_cast<int>(directx::access_vertex_buffer);
		bits &= ~graphics_only;
	}
	return static_cast<directx::D3D12_BARRIER_ACCESS>(bits);
}

auto gse::dx12::primitive_topology_of(const gpu::topology t) -> directx::D3D12_PRIMITIVE_TOPOLOGY {
	switch (t) {
		case gpu::topology::line_list: return directx::topology_line_list;
		case gpu::topology::point_list: return directx::topology_point_list;
		default: return directx::topology_triangle_list;
	}
}
