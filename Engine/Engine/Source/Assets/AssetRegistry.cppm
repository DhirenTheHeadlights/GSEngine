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

export namespace gse {
	template <typename Context>
	class asset_registry final : public non_copyable {
	public:
		explicit asset_registry(
			Context& context
		);

		~asset_registry(
		) override;

		asset_registry(
			asset_registry&&
		) noexcept = default;

		auto operator=(
			asset_registry&&
		) noexcept -> asset_registry& = default;

		template <typename T>
		auto add_loader(
		) -> resource::loader<T, Context>*;

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

		template <typename T>
		[[nodiscard]] auto loader(
			this auto&& self
		) -> decltype(auto);

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
		auto loader(
			id type_index
		) const -> resource::loader_base*;

		Context* m_context;
		asset_pipeline m_pipeline{ config::resource_path, config::baked_resource_path };
		std::unordered_map<id, std::unique_ptr<resource::loader_base>> m_resource_loaders;
	};
}

template <typename C>
gse::asset_registry<C>::asset_registry(C& context) : m_context(&context) {}

template <typename C>
gse::asset_registry<C>::~asset_registry() = default;

template <typename C>
template <typename T>
auto gse::asset_registry<C>::add_loader() -> resource::loader<T, C>* {
	const auto type_id = id_of<T>();
	assert(
		!m_resource_loaders.contains(type_id),
		std::source_location::current(),
		"Resource loader for type {} already exists.",
		type_tag<T>()
	);

	auto new_loader = std::make_unique<resource::loader<T, C>>(*m_context);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_id] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		m_pipeline.template register_type<T, C>(loader_ptr);
	}

	return loader_ptr;
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::get(const id id) const -> resource::handle<T> {
	return loader<T>()->get(id);
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->get(filename);
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::try_get(const id id) const -> resource::handle<T> {
	return loader<T>()->try_get(id);
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::try_get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->try_get(filename);
}

template <typename C>
template <typename T, typename... Args>
auto gse::asset_registry<C>::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader<T>()->queue(name, std::forward<Args>(args)...);
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::instantly_load(const resource::handle<T>& handle) -> void {
	loader<T>()->instantly_load(handle.id());
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::add(T&& resource) -> resource::handle<T> {
	return loader<T>()->add(std::forward<T>(resource));
}

template <typename C>
auto gse::asset_registry<C>::process_resource_queue() -> void {
	for (const auto& l : std::views::values(m_resource_loaders)) {
		l->flush();
	}
}

template <typename C>
auto gse::asset_registry<C>::finalize_pending_loads() -> void {
	const auto pending = m_context->take_pending_finalizations();
	for (const auto& [type_id, resource_id] : pending) {
		if (const auto it = m_resource_loaders.find(type_id); it != m_resource_loaders.end()) {
			it->second->update_state(resource_id, resource::state::loaded);
		}
	}
}

template <typename C>
template <typename ShaderLayout>
auto gse::asset_registry<C>::compile() -> void {
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

template <typename C>
auto gse::asset_registry<C>::compile_all() -> void {
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

template <typename C>
auto gse::asset_registry<C>::poll_assets() -> void {
	m_pipeline.poll();
}

template <typename C>
auto gse::asset_registry<C>::enable_hot_reload() -> void {
	m_pipeline.enable_hot_reload();
	log::println(log::category::assets, "Hot reload enabled");
}

template <typename C>
auto gse::asset_registry<C>::disable_hot_reload() -> void {
	m_pipeline.disable_hot_reload();
	log::println(log::category::assets, "Hot reload disabled");
}

template <typename C>
auto gse::asset_registry<C>::hot_reload_enabled() const -> bool {
	return m_pipeline.hot_reload_enabled();
}

template <typename C>
auto gse::asset_registry<C>::finalize_reloads() -> void {
	for (const auto& l : std::views::values(m_resource_loaders)) {
		l->finalize_reloads();
	}
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::resource_state(const id id) const -> resource::state {
	return loader<T>()->state_of(id);
}

template <typename C>
template <typename T>
auto gse::asset_registry<C>::loader(this auto&& self) -> decltype(auto) {
	auto* base_loader = self.loader(id_of<T>());
	return static_cast<resource::loader<T, C>*>(base_loader);
}

template <typename C>
auto gse::asset_registry<C>::loader(const id type_id) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_id), std::source_location::current(), "Resource loader for id {} does not exist.", type_id.number());
	return m_resource_loaders.at(type_id).get();
}

template <typename C>
auto gse::asset_registry<C>::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
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

template <typename C>
auto gse::asset_registry<C>::shutdown() -> void {
	for (auto& l : std::views::values(m_resource_loaders)) {
		l.reset();
	}
	m_resource_loaders.clear();
}
