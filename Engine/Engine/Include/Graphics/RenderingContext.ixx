export module gse.graphics:rendering_context;

import std;

import :resource_loader;

import gse.platform;
import gse.utility;

export namespace gse::renderer {
	class context final : public non_copyable {
	public:
		context();
		~context() override;

		template <typename T> auto add_loader() -> T*;
		template <typename T> auto resource(const id& id) -> typename T::handle;
		template <typename T> auto queue(const std::filesystem::path& path) -> id;
		auto flush_all_queues() -> void;

		template <typename T> auto resource_state(const id& id) -> resource_loader_base::state;
		template <typename T> auto loader() -> resource_loader_base*;
		auto config() const -> vulkan::config&;
	private:
		std::unique_ptr<vulkan::config> m_config;
		std::unordered_map<std::type_index, std::unique_ptr<resource_loader_base>> m_resource_loaders;
	};
}

gse::renderer::context::context() : m_config(platform::initialize()) {}

gse::renderer::context::~context() {
	for (auto& loader : m_resource_loaders | std::views::values) {
		loader.reset();
	}

	m_config.reset();
}

template <typename T>
auto gse::renderer::context::add_loader() -> T* {
	const auto type_index = std::type_index(typeid(T));
	assert(!m_resource_loaders.contains(type_index), std::format("Resource loader for type {} already exists.", type_index.name()));
	m_resource_loaders[type_index] = std::make_unique<T>(*this);
	return m_resource_loaders[type_index].get();
}

template <typename T>
auto gse::renderer::context::resource(const id& id) -> typename T::handle {
	const auto type_index = std::type_index(typeid(T));
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return std::any_cast<typename T::handle>(m_resource_loaders[type_index]->get(id));
}

template <typename T>
auto gse::renderer::context::queue(const std::filesystem::path& path) -> id {
	const auto type_index = std::type_index(typeid(T));
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return m_resource_loaders[type_index]->queue(path);
}

auto gse::renderer::context::flush_all_queues() -> void {
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->flush();
	}
}

template <typename T>
auto gse::renderer::context::resource_state(const id& id) -> resource_loader_base::state {
	const auto type_index = std::type_index(typeid(T));
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return m_resource_loaders[type_index]->resource_state(id);
}

template <typename T>
auto gse::renderer::context::loader() -> resource_loader_base* {
	const auto type_index = std::type_index(typeid(T));
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return m_resource_loaders[type_index].get();
}

auto gse::renderer::context::config() const -> vulkan::config& {
	assert(m_config.get(), "Vulkan config is not initialized.");
	return *m_config;
}