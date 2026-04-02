export module gse.platform:gpu_resource_manager;

import std;

import :resource_loader;
import :asset_pipeline;
import :shader_layout_compiler;
import :gpu_device;

import gse.log;
import gse.utility;

export namespace gse::gpu {
	class resource_manager final : public non_copyable {
	public:
		explicit resource_manager(device& device);

		using command = std::function<void(resource_manager&)>;

		template <typename T>
		auto add_loader(
		) -> resource::loader<T, resource_manager>*;

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

		template <typename Resource>
		auto queue_gpu_command(
			Resource* resource,
			std::function<void(resource_manager&, Resource&)> work
		) const -> void;

		template <typename T>
		auto instantly_load(
			const resource::handle<T>& handle
		) -> void;

		template <typename T>
		auto add(
			T&& resource
		) -> resource::handle<T>;

		auto process_resource_queue(
		) -> void;

		auto process_gpu_queue(
		) -> void;

		auto compile(
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

		template <typename T>
		[[nodiscard]] auto resource_state(
			id id
		) const -> resource::state;

		template <typename T>
		[[nodiscard]] auto loader(
			this auto&& self
		) -> decltype(auto);

		[[nodiscard]] auto gpu_queue_size(
		) const -> size_t;

		auto mark_pending_for_finalization(
			const std::type_index& resource_type,
			id resource_id
		) const -> void;

		auto wait_idle(
		) const -> void;

		[[nodiscard]] auto device_ref(
			this auto& self
		) -> auto&;

		auto shutdown(
		) -> void;

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;

	private:
		auto loader(
			const std::type_index& type_index
		) const -> resource::loader_base*;

		device& m_device;
		asset_pipeline m_pipeline{ config::resource_path, config::baked_resource_path };
		std::unordered_map<std::type_index, std::unique_ptr<resource::loader_base>> m_resource_loaders;

		mutable std::vector<command> m_command_queue;
		mutable std::vector<std::pair<std::type_index, id>> m_pending_gpu_resources;
		mutable std::recursive_mutex m_mutex;
	};
}

gse::gpu::resource_manager::resource_manager(device& device) : m_device(device) {}

template <typename T>
auto gse::gpu::resource_manager::add_loader() -> resource::loader<T, resource_manager>* {
	const auto type_index = std::type_index(typeid(T));
	assert(
		!m_resource_loaders.contains(type_index),
		std::source_location::current(),
		"Resource loader for type {} already exists.",
		type_index.name()
	);

	auto new_loader = std::make_unique<resource::loader<T, resource_manager>>(*this);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_index] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		m_pipeline.register_type<T, resource_manager>(loader_ptr);
	}

	return loader_ptr;
}

template <typename T>
auto gse::gpu::resource_manager::get(id id) const -> resource::handle<T> {
	return loader<T>()->get(id);
}

template <typename T>
auto gse::gpu::resource_manager::get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->get(filename);
}

template <typename T>
auto gse::gpu::resource_manager::try_get(id id) const -> resource::handle<T> {
	return loader<T>()->try_get(id);
}

template <typename T>
auto gse::gpu::resource_manager::try_get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->try_get(filename);
}

template <typename T, typename... Args>
auto gse::gpu::resource_manager::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader<T>()->queue(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::gpu::resource_manager::queue_gpu_command(Resource* resource, std::function<void(resource_manager&, Resource&)> work) const -> void {
	command final_command = [resource, work_lambda = std::move(work)](resource_manager& mgr) {
		work_lambda(mgr, *resource);
	};

	std::lock_guard lock(m_mutex);
	m_command_queue.push_back(std::move(final_command));
}

template <typename T>
auto gse::gpu::resource_manager::instantly_load(const resource::handle<T>& handle) -> void {
	loader<T>()->instantly_load(handle.id());
}

template <typename T>
auto gse::gpu::resource_manager::add(T&& resource) -> resource::handle<T> {
	return loader<T>()->add(std::forward<T>(resource));
}

auto gse::gpu::resource_manager::process_resource_queue() -> void {
	for (const auto& l : m_resource_loaders | std::views::values) {
		l->flush();
	}
}

auto gse::gpu::resource_manager::process_gpu_queue() -> void {
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
		loader(type)->update_state(resource_id, resource::state::loaded);
	}
}

auto gse::gpu::resource_manager::compile() -> void {
	m_pipeline.register_compiler_only<shader_layout>();

	if (const auto result = m_pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
		log::println(
			result.failure_count > 0 ? log::level::warning : log::level::info,
			log::category::assets,
			"Compiled {} assets ({} skipped, {} failed)",
			result.success_count, result.skipped_count, result.failure_count
		);
	}

	m_device.load_shader_layouts();
}

auto gse::gpu::resource_manager::poll_assets() -> void {
	m_pipeline.poll();
}

auto gse::gpu::resource_manager::enable_hot_reload() -> void {
	m_pipeline.enable_hot_reload();
	log::println(log::category::assets, "Hot reload enabled");
}

auto gse::gpu::resource_manager::disable_hot_reload() -> void {
	m_pipeline.disable_hot_reload();
	log::println(log::category::assets, "Hot reload disabled");
}

auto gse::gpu::resource_manager::hot_reload_enabled() const -> bool {
	return m_pipeline.hot_reload_enabled();
}

auto gse::gpu::resource_manager::finalize_reloads() -> void {
	for (const auto& l : m_resource_loaders | std::views::values) {
		l->finalize_reloads();
	}
}

template <typename T>
auto gse::gpu::resource_manager::resource_state(const id id) const -> resource::state {
	return loader<T>()->state_of(id);
}

template <typename T>
auto gse::gpu::resource_manager::loader(this auto&& self) -> decltype(auto) {
	const auto type_index = std::type_index(typeid(T));
	auto* base_loader = self.loader(type_index);
	return static_cast<resource::loader<T, resource_manager>*>(base_loader);
}

auto gse::gpu::resource_manager::gpu_queue_size() const -> size_t {
	std::lock_guard lock(m_mutex);
	return m_command_queue.size();
}

auto gse::gpu::resource_manager::mark_pending_for_finalization(const std::type_index& resource_type, id resource_id) const -> void {
	std::lock_guard lock(m_mutex);
	m_pending_gpu_resources.emplace_back(resource_type, resource_id);
}

auto gse::gpu::resource_manager::wait_idle() const -> void {
	m_device.wait_idle();
}

auto gse::gpu::resource_manager::device_ref(this auto& self) -> auto& {
	return self.m_device;
}

auto gse::gpu::resource_manager::shutdown() -> void {
	for (auto& l : m_resource_loaders | std::views::values) {
		l.reset();
	}
	m_resource_loaders.clear();
}

auto gse::gpu::resource_manager::loader(const std::type_index& type_index) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_index), std::source_location::current(), "Resource loader for type {} does not exist.", type_index.name());
	return m_resource_loaders.at(type_index).get();
}

auto gse::gpu::resource_manager::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
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
