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

	[[nodiscard]] auto state_from_access(
		gpu::access_flags access
	) -> directx::D3D12_RESOURCE_STATES;

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

auto gse::dx12::state_from_access(const gpu::access_flags access) -> directx::D3D12_RESOURCE_STATES {
	if (access.test(gpu::access_flag::depth_stencil_attachment_write) || access.test(gpu::access_flag::depth_stencil_attachment_read)) {
		return directx::resource_state_depth_write;
	}
	if (access.test(gpu::access_flag::color_attachment_write)) {
		return directx::resource_state_render_target;
	}
	if (access.test(gpu::access_flag::shader_storage_write) || access.test(gpu::access_flag::shader_write)) {
		return directx::resource_state_unordered_access;
	}
	if (access.test(gpu::access_flag::transfer_write)) {
		return directx::resource_state_copy_dest;
	}
	if (access.test(gpu::access_flag::transfer_read)) {
		return directx::resource_state_copy_source;
	}
	if (access.test(gpu::access_flag::shader_read) || access.test(gpu::access_flag::shader_sampled_read) || access.test(gpu::access_flag::shader_storage_read)) {
		return directx::resource_state_shader_resource;
	}
	return directx::resource_state_common;
}

auto gse::dx12::primitive_topology_of(const gpu::topology t) -> directx::D3D12_PRIMITIVE_TOPOLOGY {
	switch (t) {
		case gpu::topology::line_list: return directx::topology_line_list;
		case gpu::topology::point_list: return directx::topology_point_list;
		default: return directx::topology_triangle_list;
	}
}
