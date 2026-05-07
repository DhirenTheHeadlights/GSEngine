export module gse.assets:resource_loader;

import std;

import gse.core;

import :resource_handle;

export namespace gse::asset {
	struct loader_runtime {
		using gpu_submit_fn = std::function<void(std::function<void(void*)>)>;
		using finalization_push_fn = std::function<void(id, id)>;
		using gpu_wait_fn = std::function<void()>;

		gpu_submit_fn async_submit;
		gpu_submit_fn sync_submit;
		finalization_push_fn finalization_pusher;
		gpu_wait_fn gpu_waiter;
	};
}

export namespace gse::resource {
	class loader_base {
	public:
		virtual ~loader_base(
		) = default;

		virtual auto flush(
		) -> void = 0;

		virtual auto update_state(
			id resource_id,
			state new_state
		) -> void = 0;

		virtual auto finalize_reloads(
		) -> void = 0;
	};
}
