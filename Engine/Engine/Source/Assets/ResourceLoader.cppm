export module gse.assets:resource_loader;

import std;

import gse.core;

import :resource_handle;

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

		virtual auto queue_reload_by_path(
			const std::filesystem::path& baked_path
		) -> void = 0;

		virtual auto queue_by_path(
			const std::filesystem::path& baked_path
		) -> void = 0;

		virtual auto finalize_reloads(
		) -> void = 0;

		virtual auto set_pre_load_fn(
			std::function<void(const std::filesystem::path&)> fn
		) -> void = 0;
	};

	template <typename Resource>
	class loader_t : public loader_base {
	public:
		virtual auto get(
			id id
		) const -> handle<Resource> = 0;

		virtual auto get(
			const std::string& filename_no_ext
		) const -> handle<Resource> = 0;

		virtual auto try_get(
			id id
		) const -> handle<Resource> = 0;

		virtual auto try_get(
			const std::string& filename_no_ext
		) const -> handle<Resource> = 0;

		virtual auto instantly_load(
			id resource_id
		) -> void = 0;

		[[nodiscard]] virtual auto state_of(
			id resource_id
		) const -> state = 0;

		virtual auto add(
			std::unique_ptr<Resource> resource
		) -> handle<Resource> = 0;

		virtual auto enqueue(
			const std::string& name,
			std::unique_ptr<Resource> resource
		) -> handle<Resource> = 0;
	};
}
