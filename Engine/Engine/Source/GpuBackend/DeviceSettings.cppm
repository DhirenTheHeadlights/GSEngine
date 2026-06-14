export module gse.gpu_backend:device_settings;

import std;

import gse.meta;

import :enums;

export namespace gse::gpu {
	struct device_settings {
		[[
			= gse::settings::describe<"GPU backend to initialize on startup (vulkan or dx12). Requires a restart.">{},
			= gse::settings::restart_required{}
		]]
		gpu_backend_kind backend = gpu_backend_kind::vulkan;

		[[
			= gse::settings::describe<"Track GPU resource lifetimes for leak detection and faulting diagnostics.">{}
		]]
		bool tracking_enabled = false;

		[[
			= gse::settings::describe<"Attach debug names to GPU resources so they appear by name in RenderDoc, NSight, and validation messages.">{}
		]]
		bool name_resources = false;
	};
}
