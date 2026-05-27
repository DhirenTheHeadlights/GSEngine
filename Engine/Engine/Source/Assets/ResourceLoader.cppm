export module gse.assets:resource_loader;

import std;

import gse.core;

import :resource_handle;

export namespace gse::resource {
	class loader_base {
	public:
		virtual ~loader_base() = default;

		virtual auto flush() -> void = 0;

		virtual auto update_state(
			id resource_id,
			state new_state
		) -> void = 0;

		virtual auto finalize_reloads() -> void = 0;
	};
}
