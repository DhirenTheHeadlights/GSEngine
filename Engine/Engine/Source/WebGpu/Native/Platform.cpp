module gse.webgpu:platform_impl;

import std;

import gse.log;
import gse.os;
import gse.webgpu_api;
import gse.wgpu_native_api;
import gse.win32;

import :platform;

namespace gse::webgpu {
	auto on_native_log(
		WGPULogLevel level,
		WGPUStringView message,
		void* userdata
	) -> void;
}

auto gse::webgpu::on_native_log(const WGPULogLevel level, const WGPUStringView message, void*) -> void {
	const bool error = level == WGPULogLevel_Error;
	log::println(error ? log::level::error : log::level::warning, log::category::render, "wgpu-native: {}", std::string_view(message.data, message.length));
	if (error) {
		log::flush();
	}
}

auto gse::webgpu::install_platform_logging() -> void {
	wgpuSetLogCallback(&on_native_log, nullptr);
	wgpuSetLogLevel(WGPULogLevel_Warn);
}

auto gse::webgpu::create_platform_surface(const WGPUInstance instance, const native_window_handle handle, const WGPUStringView label) -> WGPUSurface {
	WGPUSurfaceSourceWindowsHWND source{
		.chain = {
			.next = nullptr,
			.sType = WGPUSType_SurfaceSourceWindowsHWND,
		},
		.hinstance = win32::GetModuleHandleW(nullptr),
		.hwnd = handle.value,
	};
	const WGPUSurfaceDescriptor desc{
		.nextInChain = &source.chain,
		.label = label,
	};
	return wgpuInstanceCreateSurface(instance, &desc);
}
