export module gse.gpu:backend_state;

import gse.gpu_backend;

export namespace gse::gpu {
	inline backend_kind active_backend = backend_kind::vulkan;
}
