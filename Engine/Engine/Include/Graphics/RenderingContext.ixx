export module gse.graphics:rendering_context;

import std;

import :resource_loader;
import :camera;

import gse.platform;
import gse.utility;

export namespace gse::renderer {
	class context final : public non_copyable {
	public:
		context(const std::string& window_title);
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

		template <typename Resource>
		auto queue_gpu_command(Resource* resource, std::function<void(context&, Resource&)> work) const -> void;

		[[nodiscard]] auto execute_and_detect_gpu_queue(const std::function<void(const context&)>& work) const -> bool;

		template <typename T>
		auto instantly_load(const resource::handle<T>& handle) -> void;

		template <typename T>
		auto add(T&& resource) -> resource::handle<T>;

		auto process_resource_queue() -> void;

		auto process_gpu_queue() -> void;

		auto compile() -> void;

		template <typename T>
		[[nodiscard]] auto resource_state(const id& id) const -> resource::state;

		template <typename T>
		[[nodiscard]] auto loader() -> resource::loader<T, context>*;

		template <typename T>
		[[nodiscard]] auto loader() const -> const resource::loader<T, context>*;

		[[nodiscard]] auto config() const -> const vulkan::config&;

		[[nodiscard]] auto config() -> vulkan::config&;

		[[nodiscard]] auto camera() -> camera&;

		[[nodiscard]] auto window() -> window&;

		[[nodiscard]] auto gpu_queue_size() const -> size_t;

		auto mark_pending_for_finalization(const std::type_index& resource_type, const id& resource_id) const -> void;

		auto set_ui_focus(bool focus) -> void;
		[[nodiscard]] auto ui_focus() const -> bool;

		auto shutdown() -> void;
	private:
		auto loader(const std::type_index& type_index) const -> resource::loader_base*;

		gse::window m_window;
		std::unique_ptr<vulkan::config> m_config;
		std::unordered_map<std::type_index, std::unique_ptr<resource::loader_base>> m_resource_loaders;

		mutable std::vector<command> m_command_queue;
		mutable std::vector<std::pair<std::type_index, id>> m_pending_gpu_resources;
		mutable std::recursive_mutex m_mutex;

		gse::camera m_camera;
		bool m_ui_focus = false;
	};
}

gse::renderer::context::context(const std::string& window_title) : m_window(window_title), m_config(vulkan::initialize(m_window.raw_handle())) {}

gse::renderer::context::~context() {
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

template <typename Resource>
auto gse::renderer::context::queue_gpu_command(Resource* resource, std::function<void(context&, Resource&)> work) const -> void {
	command final_command = [resource, work_lambda = std::move(work)](context& ctx) {
		work_lambda(ctx, *resource);
		};

	std::lock_guard lock(m_mutex);
	m_command_queue.push_back(std::move(final_command));
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

auto gse::renderer::context::execute_and_detect_gpu_queue(const std::function<void(const context&)>& work) const -> bool {
	std::lock_guard lock(m_mutex);
	const size_t queue_size_before = m_command_queue.size();

	work(*this);

	const size_t queue_size_after = m_command_queue.size();
	return queue_size_after > queue_size_before;
}

auto gse::renderer::context::process_resource_queue() -> void {
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->flush();
	}
}

auto gse::renderer::context::process_gpu_queue() -> void {
	std::vector<command> commands_to_run;
	std::vector<std::pair<std::type_index, id>> resources_to_finalize;

	{
		std::lock_guard lock(m_mutex);
		commands_to_run.swap(m_command_queue);
		resources_to_finalize.swap(m_pending_gpu_resources);
	}

	for (auto& cmd : commands_to_run) {
		if (cmd) {
			cmd(*this);
		}
	}

	for (const auto& [type, resource_id] : resources_to_finalize) {
		resource::loader_base* specific_loader = this->loader(type);
		specific_loader->update_state(resource_id, resource::state::loaded);
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

auto gse::renderer::context::window() -> gse::window& {
	return m_window;
}

auto gse::renderer::context::gpu_queue_size() const -> size_t {
	std::lock_guard lock(m_mutex);
	return m_command_queue.size();
}

auto gse::renderer::context::mark_pending_for_finalization(const std::type_index& resource_type,
	const id& resource_id) const -> void {
	std::lock_guard lock(m_mutex);
	m_pending_gpu_resources.emplace_back(resource_type, resource_id);
}

auto gse::renderer::context::set_ui_focus(const bool focus) -> void {
	m_ui_focus = focus;
}

auto gse::renderer::context::ui_focus() const -> bool {
	return m_ui_focus;
}

auto gse::renderer::context::shutdown() -> void {
	if (!m_config) return;

	m_window.shutdown();

	m_config->device_config().device.waitIdle();

	for (auto& loader : m_resource_loaders | std::views::values) {
		loader.reset();
	}

	m_resource_loaders.clear();

	m_config->swap_chain_config().albedo_image = {};
	m_config->swap_chain_config().normal_image = {};
	m_config->swap_chain_config().depth_image = {};
}

auto gse::renderer::context::loader(const std::type_index& type_index) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_index), std::format("Resource loader for type {} does not exist.", type_index.name()));
	return m_resource_loaders.at(type_index).get();
}