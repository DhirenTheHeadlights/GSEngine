export module gse.graphics:rendering_context;

import std;

import :resource_loader;
import :camera;

import gse.platform;
import gse.utility;

export namespace gse::renderer {
	class context final : public non_copyable {
	public:
		context();
		~context() override;

		using command = std::function<void(context&)>;

		template <typename T>
		auto add_loader() -> resource::loader<T, context>*;

		template <typename T>
		auto get(const id& id) const -> resource::handle<T>;

		template <typename T>
		auto get(const std::string& filename) const -> resource::handle<T>;

		template <typename T, typename... Args>
		auto queue(const std::string& name, Args&&... args) -> resource::handle<T>; 

		auto queue(command&& cmd) const -> void;

		template <typename T>
		auto instantly_load(const resource::handle<T>& handle) -> void;

		template <typename T>
		auto add(T&& resource) -> resource::handle<T>;

		auto flush_queues() -> void;

		auto compile() -> void;

		template <typename T>
		auto resource_state(const id& id) const -> resource::state;

		template <typename T>
		auto loader() -> resource::loader<T, context>*;

		template <typename T>
		auto loader() const -> const resource::loader<T, context>* ;

		auto config() const -> const vulkan::config&;

		auto config() -> vulkan::config& ;

		auto camera() -> camera&;
	private:
		auto loader(const std::type_index& type_index) const -> resource::loader_base*;

		std::unique_ptr<vulkan::config> m_config;
		std::unordered_map<std::type_index, std::unique_ptr<resource::loader_base>> m_resource_loaders;
		mutable std::vector<command> m_command_queue;
		gse::camera m_camera;
		std::mutex m_mutex;
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
auto gse::renderer::context::add_loader() -> resource::loader<T, context>* {
	const auto type_index = std::type_index(typeid(T));
	assert(!m_resource_loaders.contains(type_index), std::format("Resource loader for type {} already exists.", type_index.name()));

	auto new_loader = std::make_unique<resource::loader<T, context>>(*this);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_index] = std::move(new_loader);

	return loader_ptr;
}

template <typename T>
auto gse::renderer::context::get(const id& id) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->get(id);
}

template <typename T>
auto gse::renderer::context::get(const std::string& filename) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->get(filename);
}

template <typename T, typename... Args>
auto gse::renderer::context::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->queue(name, std::forward<Args>(args)...);
}

auto gse::renderer::context::queue(command&& cmd) const -> void {
	m_command_queue.push_back(std::move(cmd));
}

template <typename T>
auto gse::renderer::context::instantly_load(const resource::handle<T>& handle) -> void {
	auto* specific_loader = loader<T>();
	specific_loader->instantly_load(handle.id());
}

template <typename T>
auto gse::renderer::context::add(T&& resource) -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->add(std::forward<T>(resource));
}

auto gse::renderer::context::flush_queues() -> void {
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->flush();
	}

	std::vector<command> commands_to_run;

	{
		std::lock_guard lock(m_mutex);
		commands_to_run.swap(m_command_queue);
	}

	for (auto& cmd : commands_to_run) {
		if (cmd) {
			cmd(*this);
		}
	}
}

auto gse::renderer::context::compile() -> void {
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->compile();
	}
}

template <typename T>
auto gse::renderer::context::resource_state(const id& id) const -> resource::state {
	const auto type_index = std::type_index(typeid(T));
	const auto* loader = this->loader(type_index);
	return loader->resource_state(id);
}

template <typename T>
auto gse::renderer::context::loader() -> resource::loader<T, context>* {
	const auto type_index = std::type_index(typeid(T));
	auto* base_loader = loader(type_index);
	return static_cast<resource::loader<T, context>*>(base_loader);
}

template <typename T>
auto gse::renderer::context::loader() const -> const resource::loader<T, context>* {
	const auto type_index = std::type_index(typeid(T));
	auto* base_loader = loader(type_index);
	return static_cast<const resource::loader<T, context>*>(base_loader);
}

auto gse::renderer::context::config() const -> const vulkan::config& {
	assert(m_config.get(), "Vulkan config is not initialized.");
	return *m_config;
}

auto gse::renderer::context::config() -> vulkan::config& {
	assert(m_config.get(), "Vulkan config is not initialized.");
	return *m_config;
}

auto gse::renderer::context::camera() -> gse::camera& {
	return m_camera;
}

auto gse::renderer::context::loader(const std::type_index& type_index) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return m_resource_loaders.at(type_index).get();
}