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
import gse.ecs;

export namespace gse::asset {
	struct hot_reload_request {
		bool enabled = false;
	};

	struct registry {
		using gpu_submit_fn = std::function<void(std::function<void(void*)>)>;
		using finalization_push_fn = std::function<void(id, id)>;
		using gpu_wait_fn = std::function<void()>;

		struct state {
			asset_pipeline pipeline{ config::resource_path, config::baked_resource_path };
			std::unordered_map<id, std::unique_ptr<resource::loader_base>> resource_loaders;
			gpu_submit_fn async_submit;
			gpu_submit_fn sync_submit;
			finalization_push_fn finalization_pusher;
			gpu_wait_fn gpu_waiter;
		};

		static auto update(
			const update_context& ctx,
			state& s
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			state& s
		) -> void;

		static auto apply_finalizations(
			state& s,
			std::span<const std::pair<id, id>> finalizations
		) -> void;

		template <typename T>
		static auto add_loader(
			state& s
		) -> resource::loader_t<T>*;

		template <typename T>
		static auto get(
			const state& s,
			id id
		) -> resource::handle<T>;

		template <typename T>
		static auto get(
			const state& s,
			const std::string& filename
		) -> resource::handle<T>;

		template <typename T>
		static auto try_get(
			const state& s,
			id id
		) -> resource::handle<T>;

		template <typename T>
		static auto try_get(
			const state& s,
			const std::string& filename
		) -> resource::handle<T>;

		template <typename T, typename... Args>
		static auto queue(
			state& s,
			const std::string& name,
			Args&&... args
		) -> resource::handle<T>;

		template <typename T>
		static auto instantly_load(
			state& s,
			const resource::handle<T>& handle
		) -> void;

		template <typename T>
		static auto add(
			state& s,
			T&& resource
		) -> resource::handle<T>;

		template <typename T>
		[[nodiscard]] static auto resource_state(
			const state& s,
			id id
		) -> resource::state;

		template <typename ShaderLayout>
		static auto compile(
			state& s
		) -> void;

		static auto compile_all(
			state& s
		) -> void;

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;

	private:
		template <typename T>
		static auto loader_for(
			const state& s
		) -> resource::loader_t<T>*;

		static auto loader_base_for(
			const state& s,
			id type_index
		) -> resource::loader_base*;
	};

	struct load_ctx {
		registry::state& assets;
		std::function<void(std::function<void(void*)>)> submit_gpu_work;
	};
}

export namespace gse::resource {
	template <typename Resource>
	class loader final : public loader_t<Resource>, public non_copyable {
	public:
		explicit loader(
			asset::registry::state& s
		);

		~loader(
		) override = default;

		auto flush(
		) -> void override;

		auto update_state(
			id resource_id,
			state new_state
		) -> void override;

		auto queue_reload(
			id resource_id
		) -> void;

		auto queue_reload_by_path(
			const std::filesystem::path& baked_path
		) -> void override;

		auto queue_by_path(
			const std::filesystem::path& baked_path
		) -> void override;

		auto finalize_reloads(
		) -> void override;

		auto set_pre_load_fn(
			std::function<void(const std::filesystem::path&)> fn
		) -> void override;

		auto get(
			id id
		) const -> handle<Resource> override;

		auto get(
			const std::string& filename_no_ext
		) const -> handle<Resource> override;

		auto try_get(
			id id
		) const -> handle<Resource> override;

		auto try_get(
			const std::string& filename_no_ext
		) const -> handle<Resource> override;

		auto instantly_load(
			id resource_id
		) -> void override;

		[[nodiscard]] auto state_of(
			id resource_id
		) const -> state override;

		auto add(
			std::unique_ptr<Resource> resource
		) -> handle<Resource> override;

		auto enqueue(
			const std::string& name,
			std::unique_ptr<Resource> resource
		) -> handle<Resource> override;
	private:
		asset::registry::state& m_state;
		id_mapped_collection<std::unique_ptr<resource_slot<Resource>>> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		task::group m_load_group{ generate_id("resource.loader.load") };
		mutable std::mutex m_mutex;

		std::vector<id> m_pending_reloads;
		std::mutex m_reload_mutex;

		std::function<void(const std::filesystem::path&)> m_pre_load_fn;

		auto slot_ptr(
			this auto&& self,
			id id
		) -> resource_slot<Resource>*;
	};
}

auto gse::asset::registry::update(const update_context& ctx, state& s) -> async::task<> {
	for (const auto& req : ctx.read_channel<hot_reload_request>()) {
		if (req.enabled == s.pipeline.hot_reload_enabled()) {
			continue;
		}
		if (req.enabled) {
			s.pipeline.enable_hot_reload();
			log::println(log::category::assets, "Hot reload enabled");
		}
		else {
			s.pipeline.disable_hot_reload();
			log::println(log::category::assets, "Hot reload disabled");
		}
	}

	s.pipeline.poll();

	for (const auto& l : std::views::values(s.resource_loaders)) {
		l->flush();
	}

	co_return;
}

auto gse::asset::registry::shutdown(shutdown_context&, state& s) -> void {
	for (auto& l : std::views::values(s.resource_loaders)) {
		l.reset();
	}
	s.resource_loaders.clear();
}

auto gse::asset::registry::apply_finalizations(state& s, const std::span<const std::pair<id, id>> finalizations) -> void {
	for (const auto& [type_id, resource_id] : finalizations) {
		if (const auto it = s.resource_loaders.find(type_id); it != s.resource_loaders.end()) {
			it->second->update_state(resource_id, resource::state::loaded);
		}
	}
}

template <typename T>
auto gse::asset::registry::add_loader(state& s) -> resource::loader_t<T>* {
	const auto type_id = id_of<T>();
	assert(
		!s.resource_loaders.contains(type_id),
		"Resource loader for type {} already exists.",
		type_tag<T>()
	);

	auto new_loader = std::make_unique<resource::loader<T>>(s);
	auto* loader_ptr = new_loader.get();
	s.resource_loaders[type_id] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		s.pipeline.template register_type<T>(loader_ptr);
	}

	return loader_ptr;
}

template <typename T>
auto gse::asset::registry::get(const state& s, const id id) -> resource::handle<T> {
	return loader_for<T>(s)->get(id);
}

template <typename T>
auto gse::asset::registry::get(const state& s, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(s)->get(filename);
}

template <typename T>
auto gse::asset::registry::try_get(const state& s, const id id) -> resource::handle<T> {
	return loader_for<T>(s)->try_get(id);
}

template <typename T>
auto gse::asset::registry::try_get(const state& s, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(s)->try_get(filename);
}

template <typename T, typename... Args>
auto gse::asset::registry::queue(state& s, const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader_for<T>(s)->enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
}

template <typename T>
auto gse::asset::registry::instantly_load(state& s, const resource::handle<T>& handle) -> void {
	loader_for<T>(s)->instantly_load(handle.id());
}

template <typename T>
auto gse::asset::registry::add(state& s, T&& resource) -> resource::handle<T> {
	return loader_for<T>(s)->add(std::make_unique<T>(std::forward<T>(resource)));
}

template <typename T>
auto gse::asset::registry::resource_state(const state& s, const id id) -> resource::state {
	return loader_for<T>(s)->state_of(id);
}

template <typename T>
auto gse::asset::registry::loader_for(const state& s) -> resource::loader_t<T>* {
	return static_cast<resource::loader_t<T>*>(loader_base_for(s, id_of<T>()));
}

auto gse::asset::registry::loader_base_for(const state& s, const id type_id) -> resource::loader_base* {
	assert(s.resource_loaders.contains(type_id), "Resource loader for id {} does not exist.", type_id.number());
	return s.resource_loaders.at(type_id).get();
}

template <typename ShaderLayout>
auto gse::asset::registry::compile(state& s) -> void {
	s.pipeline.template register_compiler_only<ShaderLayout>();

	if (const auto result = s.pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
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

auto gse::asset::registry::compile_all(state& s) -> void {
	if (const auto result = s.pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
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

auto gse::asset::registry::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
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

template <typename R>
gse::resource::loader<R>::loader(asset::registry::state& s) : m_state(s) {}

template <typename R>
auto gse::resource::loader<R>::set_pre_load_fn(std::function<void(const std::filesystem::path&)> fn) -> void {
	m_pre_load_fn = std::move(fn);
}

template <typename R>
auto gse::resource::loader<R>::state_of(const id resource_id) const -> state {
	std::lock_guard lock(m_mutex);
	if (const auto* s = slot_ptr(resource_id)) {
		return s->current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename R>
auto gse::resource::loader<R>::slot_ptr(this auto&& self, const id id) -> resource_slot<R>* {
	if (auto* uptr = self.m_resources.try_get(id)) {
		return uptr->get();
	}
	return nullptr;
}

template <typename R>
auto gse::resource::loader<R>::flush() -> void {
	std::vector<id> ids_to_load;

	{
		std::lock_guard lock(m_mutex);
		for (const auto& uptr : m_resources.items()) {
			if (uptr->current_state.load(std::memory_order_acquire) == state::queued) {
				uptr->current_state.store(state::loading, std::memory_order_release);

				const id rid = uptr->resource.read()
					? uptr->resource.read()->id()
					: m_path_to_id[uptr->path];

				ids_to_load.push_back(rid);
			}
		}
	}

	std::vector<gse::move_only_function<void()>> jobs;
	jobs.reserve(ids_to_load.size());

	for (const id rid : ids_to_load) {
		jobs.emplace_back([this, rid] {
			R* resource_ptr;
			std::filesystem::path path;
			{
				std::lock_guard lock(m_mutex);
				if (auto* s = slot_ptr(rid)) {
					if (!s->resource.read()) {
						s->resource.write() = std::make_unique<R>(s->path);
						s->resource.publish();
					}
					resource_ptr = s->resource.read().get();
					path = s->path;
				}
				else {
					update_state(rid, state::failed);
					return;
				}
			}

			if (m_pre_load_fn && !path.empty()) {
				m_pre_load_fn(path);
			}

			std::size_t gpu_submissions = 0;
			asset::load_ctx ctx{
				.assets = m_state,
				.submit_gpu_work = [this, &gpu_submissions](std::function<void(void*)> work) {
					++gpu_submissions;
					assert(static_cast<bool>(m_state.async_submit), "asset::registry async submitter not configured");
					m_state.async_submit(std::move(work));
				},
			};

			try {
				resource_ptr->load(ctx);
			}
			catch (...) {
				update_state(rid, state::failed);
				throw;
			}

			if (gpu_submissions > 0) {
				assert(static_cast<bool>(m_state.finalization_pusher), "asset::registry finalization pusher not configured");
				m_state.finalization_pusher(id_of<R>(), rid);
			}
			else {
				update_state(rid, state::loaded);
			}
		});
	}

	m_load_group.post_range(jobs.begin(), jobs.end(), generate_id("resource.load"));
}

template <typename R>
auto gse::resource::loader<R>::update_state(const id resource_id, const state new_state) -> void {
	std::lock_guard lock(m_mutex);
	if (auto* s = slot_ptr(resource_id)) {
		s->current_state.store(new_state, std::memory_order_release);
	}
}

template <typename R>
auto gse::resource::loader<R>::queue_reload(const id resource_id) -> void {
	std::lock_guard lock(m_reload_mutex);

	if (std::ranges::find(m_pending_reloads, resource_id) != m_pending_reloads.end()) {
		return;
	}

	m_pending_reloads.push_back(resource_id);
}

template <typename R>
auto gse::resource::loader<R>::queue_reload_by_path(const std::filesystem::path& baked_path) -> void {
	std::lock_guard lock(m_mutex);

	auto it = m_path_to_id.find(baked_path);
	if (it == m_path_to_id.end()) {
		return;
	}

	queue_reload(it->second);
}

template <typename R>
auto gse::resource::loader<R>::queue_by_path(const std::filesystem::path& baked_path) -> void {
	std::lock_guard lock(m_mutex);

	if (m_path_to_id.contains(baked_path)) {
		return;
	}

	auto temp_resource = std::make_unique<R>(baked_path);
	const id resource_id = temp_resource->id();

	auto slot = std::make_unique<resource_slot<R>>(std::move(temp_resource), state::queued, baked_path);
	if (m_resources.add(resource_id, std::move(slot))) {
		m_path_to_id[baked_path] = resource_id;
	}
}

template <typename R>
auto gse::resource::loader<R>::finalize_reloads() -> void {
	std::vector<id> reloads_to_process;
	{
		std::lock_guard lock(m_reload_mutex);
		reloads_to_process.swap(m_pending_reloads);
	}

	if (reloads_to_process.empty()) {
		return;
	}

	if (m_state.gpu_waiter) {
		m_state.gpu_waiter();
	}

	for (const id rid : reloads_to_process) {
		resource_slot<R>* s;
		{
			std::lock_guard lock(m_mutex);
			s = slot_ptr(rid);
			if (!s) {
				continue;
			}
		}

		const auto current_state = s->current_state.load(std::memory_order_acquire);
		if (current_state != state::loaded && current_state != state::reloading) {
			continue;
		}

		s->current_state.store(state::reloading, std::memory_order_release);

		auto new_resource = std::make_unique<R>(s->path);

		std::size_t gpu_submissions = 0;
		asset::load_ctx ctx{
			.assets = m_state,
			.submit_gpu_work = [this, &gpu_submissions](std::function<void(void*)> work) {
				++gpu_submissions;
				assert(static_cast<bool>(m_state.sync_submit), "asset::registry sync submitter not configured");
				m_state.sync_submit(std::move(work));
			},
		};
		new_resource->load(ctx);

		if (gpu_submissions > 0 && m_state.gpu_waiter) {
			m_state.gpu_waiter();
		}

		if (s->resource.read()) {
			auto* old_resource = const_cast<R*>(s->resource.read().get());
			old_resource->unload();
		}

		s->resource.write() = std::move(new_resource);
		s->resource.publish();
		s->version.fetch_add(1, std::memory_order_release);
		s->current_state.store(state::loaded, std::memory_order_release);

		log::println(log::category::assets, "Hot reload reloaded resource: {}", s->path.filename().string());
	}
}

template <typename R>
auto gse::resource::loader<R>::get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	assert(s, "Resource with ID {} not found in this loader.", id);
	return handle<R>(id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(resource_id);
	assert(s, "Resource with ID {} not found in this loader.", resource_id);
	return handle<R>(resource_id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::try_get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	if (!s) {
		return handle<R>{};
	}
	return handle<R>(id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::try_get(const std::string& filename_no_ext) const -> handle<R> {
	if (!gse::exists(filename_no_ext)) {
		return handle<R>{};
	}
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(resource_id);
	if (!s) {
		return handle<R>{};
	}
	return handle<R>(resource_id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::instantly_load(const id resource_id) -> void {
	resource_slot<R>* s;
	{
		std::lock_guard lock(m_mutex);
		s = slot_ptr(resource_id);
		assert(s, "invalid id");
		if (s->current_state == state::loaded) {
			return;
		}
	}

	while (s->current_state.load(std::memory_order_acquire) == state::loading) {
		std::this_thread::yield();
	}

	if (s->current_state.load(std::memory_order_acquire) == state::loaded) {
		return;
	}

	update_state(resource_id, state::loading);

	if (m_pre_load_fn && !s->path.empty()) {
		m_pre_load_fn(s->path);
	}

	if (!s->resource.read()) {
		s->resource.write() = std::make_unique<R>(s->path);
		s->resource.publish();
	}

	std::size_t gpu_submissions = 0;
	asset::load_ctx ctx{
		.assets = m_state,
		.submit_gpu_work = [this, &gpu_submissions](std::function<void(void*)> work) {
			++gpu_submissions;
			assert(static_cast<bool>(m_state.sync_submit), "asset::registry sync submitter not configured");
			m_state.sync_submit(std::move(work));
		},
	};

	try {
		const_cast<R*>(s->resource.read().get())->load(ctx);
	}
	catch (...) {
		update_state(resource_id, state::failed);
		throw;
	}

	if (gpu_submissions > 0 && m_state.gpu_waiter) {
		m_state.gpu_waiter();
	}

	update_state(resource_id, state::loaded);
}

template <typename R>
auto gse::resource::loader<R>::enqueue(const std::string& name, std::unique_ptr<R> resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			const auto* s = slot_ptr(resource_id);
			return handle<R>(resource_id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
		}
	}

	const auto resource_id = resource->id();

	auto slot = std::make_unique<resource_slot<R>>(std::move(resource), state::queued, "");
	auto* slot_raw = slot.get();
	m_resources.add(resource_id, std::move(slot));
	return handle<R>(resource_id, slot_raw, 0);
}

template <typename R>
auto gse::resource::loader<R>::add(std::unique_ptr<R> resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto id = resource->id();
	assert(!m_resources.contains(id), "Resource with ID {} already exists.", id);

	auto slot = std::make_unique<resource_slot<R>>(std::move(resource), state::loaded, "");
	auto* slot_raw = slot.get();
	m_resources.add(id, std::move(slot));

	return handle<R>(id, slot_raw, 0);
}
