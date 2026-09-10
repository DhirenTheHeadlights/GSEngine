export module gse.assets:resource_loader;

export namespace gse::resource {
	class loader_base {
	public:
		virtual ~loader_base() = default;

		virtual auto flush() -> void = 0;

		virtual auto finalize_reloads() -> void = 0;
	};
}