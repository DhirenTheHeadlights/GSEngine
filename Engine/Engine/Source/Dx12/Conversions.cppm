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

	[[nodiscard]] auto d3d12_state_of(
		gpu::resource_state state
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

auto gse::dx12::d3d12_state_of(const gpu::resource_state state) -> directx::D3D12_RESOURCE_STATES {
	switch (state) {
		case gpu::resource_state::color_target:
			return directx::resource_state_render_target;
		case gpu::resource_state::depth_write:
			return directx::resource_state_depth_write;
		case gpu::resource_state::depth_read:
			return directx::resource_state_depth_read;
		case gpu::resource_state::sampled:
		case gpu::resource_state::storage_read:
			return directx::resource_state_shader_resource;
		case gpu::resource_state::storage_write:
		case gpu::resource_state::storage_read_write:
			return directx::resource_state_unordered_access;
		case gpu::resource_state::copy_src:
			return directx::resource_state_copy_source;
		case gpu::resource_state::copy_dst:
			return directx::resource_state_copy_dest;
		case gpu::resource_state::present:
			return directx::resource_state_present;
		case gpu::resource_state::indirect:
			return directx::resource_state_indirect_argument;
		case gpu::resource_state::acceleration_structure_read:
		case gpu::resource_state::acceleration_structure_build:
			return directx::resource_state_raytracing_acceleration_structure;
		default:
			return directx::resource_state_common;
	}
}

auto gse::dx12::primitive_topology_of(const gpu::topology t) -> directx::D3D12_PRIMITIVE_TOPOLOGY {
	switch (t) {
		case gpu::topology::line_list: return directx::topology_line_list;
		case gpu::topology::point_list: return directx::topology_point_list;
		default: return directx::topology_triangle_list;
	}
}
