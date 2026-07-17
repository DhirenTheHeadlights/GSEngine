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

		[[
			= gse::settings::describe<"Enable D3D12 GPU-based validation. Requires the validation layer and instruments every "
									  "shader, so the driver recompiles all pipelines and pipeline creation stalls for seconds. "
									  "Leave off unless chasing a GPU-side violation. Requires a restart.">{},
			= gse::settings::restart_required{}
		]]
		bool gpu_based_validation = false;
	};
}
