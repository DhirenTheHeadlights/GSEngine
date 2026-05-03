export module gse.assets:registry;

import std;

import :asset_pipeline;
import :resource_loader;
import :resource_handle;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.config;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::asset {
	class context {
	public:
		virtual ~context() = default;

		virtual auto take_pending_finalizations(
		) -> std::vector<std::pair<id, id>> = 0;
	};

	class registry final : public non_copyable {
	public:
		explicit registry(
			asset::context& ctx
		);

		~registry(
		) override = default;

		template <typename T, typename Ctx>
		auto add_loader(
			Ctx& ctx
		) -> resource::loader<T, Ctx>*;

		template <typename T>
		auto get(
			id id
		) const -> resource::handle<T>;

		template <typename T>
		auto get(
			const std::string& filename
		) const -> resource::handle<T>;

		template <typename T>
		auto try_get(
			id id
		) const -> resource::handle<T>;

		template <typename T>
		auto try_get(
			const std::string& filename
		) const -> resource::handle<T>;

		template <typename T, typename... Args>
		auto queue(
			const std::string& name,
			Args&&... args
		) -> resource::handle<T>;

		template <typename T>
		auto instantly_load(
			const resource::handle<T>& handle
		) -> void;

		template <typename T>
		auto add(
			T&& resource
		) -> resource::handle<T>;

		template <typename T>
		[[nodiscard]] auto resource_state(
			id id
		) const -> resource::state;

		auto process_resource_queue(
		) -> void;

		auto finalize_pending_loads(
		) -> void;

		template <typename ShaderLayout>
		auto compile(
		) -> void;

		auto compile_all(
		) -> void;

		auto poll_assets(
		) -> void;

		auto enable_hot_reload(
		) -> void;

		auto disable_hot_reload(
		) -> void;

		[[nodiscard]] auto hot_reload_enabled(
		) const -> bool;

		auto finalize_reloads(
		) -> void;

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;

		auto shutdown(
		) -> void;
	private:
		template <typename T>
		auto loader_for(
		) const -> resource::loader_t<T>*;

		auto loader_base_for(
			id type_index
		) const -> resource::loader_base*;

		asset::context* m_context;
		asset_pipeline m_pipeline{ config::resource_path, config::baked_resource_path };
		std::unordered_map<id, std::unique_ptr<resource::loader_base>> m_resource_loaders;
	};
}

gse::asset::registry::registry(asset::context& ctx) : m_context(&ctx) {}

template <typename T, typename Ctx>
auto gse::asset::registry::add_loader(Ctx& ctx) -> resource::loader<T, Ctx>* {
	const auto type_id = id_of<T>();
	assert(
		!m_resource_loaders.contains(type_id),
		std::source_location::current(),
		"Resource loader for type {} already exists.",
		type_tag<T>()
	);

	auto new_loader = std::make_unique<resource::loader<T, Ctx>>(ctx);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_id] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		m_pipeline.template register_type<T, Ctx>(loader_ptr);
	}

	return loader_ptr;
}

template <typename T>
auto gse::asset::registry::get(const id id) const -> resource::handle<T> {
	return loader_for<T>()->get(id);
}

template <typename T>
auto gse::asset::registry::get(const std::string& filename) const -> resource::handle<T> {
	return loader_for<T>()->get(filename);
}

template <typename T>
auto gse::asset::registry::try_get(const id id) const -> resource::handle<T> {
	return loader_for<T>()->try_get(id);
}

template <typename T>
auto gse::asset::registry::try_get(const std::string& filename) const -> resource::handle<T> {
	return loader_for<T>()->try_get(filename);
}

template <typename T, typename... Args>
auto gse::asset::registry::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader_for<T>()->enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
}

template <typename T>
auto gse::asset::registry::instantly_load(const resource::handle<T>& handle) -> void {
	loader_for<T>()->instantly_load(handle.id());
}

template <typename T>
auto gse::asset::registry::add(T&& resource) -> resource::handle<T> {
	return loader_for<T>()->add(std::make_unique<T>(std::forward<T>(resource)));
}

template <typename T>
auto gse::asset::registry::resource_state(const id id) const -> resource::state {
	return loader_for<T>()->state_of(id);
}

template <typename T>
auto gse::asset::registry::loader_for() const -> resource::loader_t<T>* {
	return static_cast<resource::loader_t<T>*>(loader_base_for(id_of<T>()));
}

inline auto gse::asset::registry::loader_base_for(const id type_id) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_id), std::source_location::current(), "Resource loader for id {} does not exist.", type_id.number());
	return m_resource_loaders.at(type_id).get();
}

inline auto gse::asset::registry::process_resource_queue() -> void {
	for (const auto& l : std::views::values(m_resource_loaders)) {
		l->flush();
	}
}

inline auto gse::asset::registry::finalize_pending_loads() -> void {
	const auto pending = m_context->take_pending_finalizations();
	for (const auto& [type_id, resource_id] : pending) {
		if (const auto it = m_resource_loaders.find(type_id); it != m_resource_loaders.end()) {
			it->second->update_state(resource_id, resource::state::loaded);
		}
	}
}

template <typename ShaderLayout>
auto gse::asset::registry::compile() -> void {
	m_pipeline.template register_compiler_only<ShaderLayout>();

	if (const auto result = m_pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
		log::println(
			result.failure_count > 0 ? log::level::warning : log::level::info,
			log::category::assets,
			"Compiled {} assets ({} skipped, {} failed)",
			result.success_count,
			result.skipped_count,
			result.failure_count
		);
	}
}

inline auto gse::asset::registry::compile_all() -> void {
	if (const auto result = m_pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
		log::println(
			result.failure_count > 0 ? log::level::warning : log::level::info,
			log::category::assets,
			"Compiled {} assets ({} skipped, {} failed)",
			result.success_count,
			result.skipped_count,
			result.failure_count
		);
	}
}

inline auto gse::asset::registry::poll_assets() -> void {
	m_pipeline.poll();
}

inline auto gse::asset::registry::enable_hot_reload() -> void {
	m_pipeline.enable_hot_reload();
	log::println(log::category::assets, "Hot reload enabled");
}

inline auto gse::asset::registry::disable_hot_reload() -> void {
	m_pipeline.disable_hot_reload();
	log::println(log::category::assets, "Hot reload disabled");
}

inline auto gse::asset::registry::hot_reload_enabled() const -> bool {
	return m_pipeline.hot_reload_enabled();
}

inline auto gse::asset::registry::finalize_reloads() -> void {
	for (const auto& l : std::views::values(m_resource_loaders)) {
		l->finalize_reloads();
	}
}

inline auto gse::asset::registry::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
	std::vector<std::string> result;

	const auto dir_path = config::baked_resource_path / baked_dir;

	if (!std::filesystem::exists(dir_path)) {
		return result;
	}

	for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		if (entry.path().extension().string() != baked_ext) {
			continue;
		}

		result.push_back(entry.path().stem().string());
	}

	std::ranges::sort(result);
	return result;
}

inline auto gse::asset::registry::shutdown() -> void {
	for (auto& l : std::views::values(m_resource_loaders)) {
		l.reset();
	}
	m_resource_loaders.clear();
}
