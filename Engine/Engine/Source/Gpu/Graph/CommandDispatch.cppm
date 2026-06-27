export module gse.gpu:command_dispatch;

import gse.gpu_backend;

export namespace gse::gpu {
	struct command_dispatch;

	[[nodiscard]] auto command_dispatch_for_backend(
		gpu_backend_kind backend
	) -> const command_dispatch*;
}
