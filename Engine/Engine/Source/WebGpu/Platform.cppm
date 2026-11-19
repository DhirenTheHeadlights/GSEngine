export module gse.webgpu:platform;

import std;

import gse.os;
import gse.webgpu_api;

export namespace gse::webgpu {
	auto install_platform_logging(
	) -> void;

	[[nodiscard]] auto create_platform_surface(
		WGPUInstance instance,
		native_window_handle handle,
		WGPUStringView label
	) -> WGPUSurface;
}
