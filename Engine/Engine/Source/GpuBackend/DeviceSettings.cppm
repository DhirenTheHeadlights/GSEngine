export module gse.gpu_backend:device_settings;

import std;

import gse.meta;

export namespace gse::gpu {
	struct device_settings {
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
