export module gse.gpu:backend_state;

import gse.gpu_backend;

export namespace gse::gpu {
	inline gpu_backend_kind active_backend = gpu_backend_kind::vulkan;
}
