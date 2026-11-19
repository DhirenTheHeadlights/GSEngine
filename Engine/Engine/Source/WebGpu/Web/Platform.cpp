module gse.webgpu:platform_impl;

import std;

import gse.os;
import gse.webgpu_api;

import :platform;

auto gse::webgpu::install_platform_logging() -> void {}

auto gse::webgpu::create_platform_surface(const WGPUInstance instance, const native_window_handle, const WGPUStringView label) -> WGPUSurface {
	const WGPUSurfaceDescriptor desc{
		.nextInChain = nullptr,
		.label = label,
	};
	return wgpuInstanceCreateSurface(instance, &desc);
}
