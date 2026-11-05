module;

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

export module gse.wgpu_native_api;

export namespace gse::webgpu {
	using ::WGPULogLevel;
	using ::WGPULogLevel_Error;
	using ::WGPULogLevel_Warn;
	using ::wgpuSetLogCallback;
	using ::wgpuSetLogLevel;
}
