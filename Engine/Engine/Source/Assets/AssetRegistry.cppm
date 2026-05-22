export module gse.assets:registry;

import std;

import :resource_loader;
import :resource_handle;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.fs;

export namespace gse::resource {
	template <typename Resource>
	class loader;
}

export namespace gse::asset {
	struct hot_reload_request {
		bool enabled = false;
	};

	struct registry {
		struct data {
			std::unordered_map<id, std::unique_ptr<resource::loader_base>> resource_loaders;
			file_watcher watcher;
			std::function<void()> enable_hot_reload_fn;
			std::function<void()> disable_hot_reload_fn;
			bool hot_reload_enabled = false;
			channel_writer* channels = nullptr;
		};

		static auto run(
			run_context& ctx,
			data& d
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			data& d
		) -> void;
	};

	using data = registry::data;

	template <typename T>
	auto add_loader(
		data& d
	) -> resource::loader<T>*;

	template <typename T>
	auto get(
		const data& d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto get(
		const data& d,
		const std::string& filename
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		const data& d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		const data& d,
		const std::string& filename
	) -> resource::handle<T>;

	template <
		typename T,
		typename... Args
	>
	auto queue(
		data& d,
		const std::string& name,
		Args&&... args
	) -> resource::handle<T>;

	template <typename T>
	auto add(
		data& d,
		T&& resource
	) -> resource::handle<T>;

	template <typename T>
	[[nodiscard]] auto resource_state(
		const data& d,
		id resource_id
	) -> resource::state;

	struct load_ctx {
		data& assets;
		channel_writer& channels;
	};

	template <typename T>
	[[nodiscard]] auto load(
		run_context& ctx,
		std::string_view path
	) -> async::task<resource::handle<T>>;
}

namespace gse::asset {
	template <typename T>
	auto loader_for(
		const data& d
	) -> resource::loader<T>*;

	auto loader_base_for(
		const data& d,
		id type_index
	) -> resource::loader_base*;
}

export namespace gse::resource {
	template <typename Resource>
	class loader final : public loader_base, public non_copyable {
	public:
		explicit loader(
			asset::data& d
		);

		~loader() override = default;

		auto flush() -> void override;

		auto update_state(
			id resource_id,
			state new_state
		) -> void override;

		auto queue_reload(
			id resource_id
		) -> void;

		auto queue_reload_by_path(
			const std::filesystem::path& baked_path
		) -> void;

		auto queue_by_path(
			const std::filesystem::path& baked_path
		) -> void;

		auto finalize_reloads() -> void override;

		auto set_pre_load_fn(
			std::function<void(const std::filesystem::path&)> fn
		) -> void;

		auto get(
			id id
		) const -> handle<Resource>;

		auto get(
			const std::string& filename_no_ext
		) const -> handle<Resource>;

		auto try_get(
			id id
		) const -> handle<Resource>;

		auto try_get(
			const std::string& filename_no_ext
		) const -> handle<Resource>;

		[[nodiscard]] auto state_of(
			id resource_id
		) const -> state;

		auto add(
			std::unique_ptr<Resource> resource
		) -> handle<Resource>;

		auto enqueue(
			const std::string& name,
			std::unique_ptr<Resource> resource
		) -> handle<Resource>;

	private:
		asset::data& m_data;
		id_mapped_collection<std::unique_ptr<resource_slot<Resource>>> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		std::vector<async::task<>> m_in_flight;
		mutable std::mutex m_mutex;

		std::vector<id> m_pending_reloads;
		std::mutex m_reload_mutex;

		std::function<void(const std::filesystem::path&)> m_pre_load_fn;

		auto slot_ptr(
			this auto&& self,
			id id
		);

		auto launch_load(
			id rid
		) -> async::task<>;

		auto launch_reload(
			id rid
		) -> async::task<>;

		auto reap_done_tasks() -> void;
	};
}

auto gse::asset::registry::run(run_context& ctx, data& d) -> async::task<> {
	d.channels = &ctx.channels;

	while (true) {
		for (const auto& req : ctx.read_channel<hot_reload_request>()) {
			if (req.enabled == d.hot_reload_enabled) {
				continue;
			}
			if (req.enabled) {
				if (d.enable_hot_reload_fn) {
					d.enable_hot_reload_fn();
				}
				log::println(log::category::assets, "Hot reload enabled");
			}
			else {
				if (d.disable_hot_reload_fn) {
					d.disable_hot_reload_fn();
				}
				log::println(log::category::assets, "Hot reload disabled");
			}
			d.hot_reload_enabled = req.enabled;
		}

		d.watcher.poll();

		co_await ctx.next_tick();

		for (const auto& l : std::views::values(d.resource_loaders)) {
			l->flush();
		}
	}
}

auto gse::asset::registry::shutdown(shutdown_context&, data& d) -> void {
	gse::task::wait_idle();
	for (auto& l : std::views::values(d.resource_loaders)) {
		l.reset();
	}
	d.resource_loaders.clear();
	d.channels = nullptr;
}

template <typename T>
auto gse::asset::add_loader(data& d) -> resource::loader<T>* {
	const auto type_id = id_of<T>();
	assert(!d.resource_loaders.contains(type_id), "Resource loader for type {} already exists.", type_tag<T>());

	auto new_loader = std::make_unique<resource::loader<T>>(d);
	auto* loader_ptr = new_loader.get();
	d.resource_loaders[type_id] = std::move(new_loader);

	return loader_ptr;
}

template <typename T>
auto gse::asset::get(const data& d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->get(resource_id);
}

template <typename T>
auto gse::asset::get(const data& d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->get(filename);
}

template <typename T>
auto gse::asset::try_get(const data& d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(resource_id);
}

template <typename T>
auto gse::asset::try_get(const data& d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(filename);
}

template <typename T, typename... Args>
auto gse::asset::queue(data& d, const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader_for<T>(d)->enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
}

template <typename T>
auto gse::asset::add(data& d, T&& resource) -> resource::handle<T> {
	return loader_for<T>(d)->add(std::make_unique<T>(std::forward<T>(resource)));
}

template <typename T>
auto gse::asset::resource_state(const data& d, const id resource_id) -> resource::state {
	return loader_for<T>(d)->state_of(resource_id);
}

template <typename T>
auto gse::asset::loader_for(const data& d) -> resource::loader<T>* {
	return static_cast<resource::loader<T>*>(loader_base_for(d, id_of<T>()));
}

auto gse::asset::loader_base_for(const data& d, const id type_id) -> resource::loader_base* {
	assert(d.resource_loaders.contains(type_id), "Resource loader for id {} does not exist.", type_id.number());
	return d.resource_loaders.at(type_id).get();
}

template <typename T>
auto gse::asset::load(run_context& ctx, const std::string_view path) -> async::task<resource::handle<T>> {
	auto* assets_ptr = static_cast<data*>(ctx.states.state_ptr(id_of<registry>()));
	assert(
		assets_ptr != nullptr,
		"asset::load: asset::registry must be added before any system that calls asset::load"
	);
	auto& assets = *assets_ptr;
	auto handle = get<T>(assets, std::string(path));

	while (resource_state<T>(assets, handle.id()) != resource::state::loaded) {
		co_await ctx.yield_tick();
	}

	co_return handle;
}

template <typename R>
gse::resource::loader<R>::loader(asset::data& d) : m_data(d) {
}

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
auto gse::resource::loader<R>::slot_ptr(this auto&& self, const id id) {
	auto* uptr = self.m_resources.try_get(id);
	return uptr ? uptr->get() : nullptr;
}

template <typename R>
auto gse::resource::loader<R>::reap_done_tasks() -> void {
	std::erase_if(m_in_flight, [](const async::task<>& t) {
		return t.done();
	});
}

template <typename R>
auto gse::resource::loader<R>::flush() -> void {
	reap_done_tasks();

	std::vector<id> ids_to_load;
	{
		std::lock_guard lock(m_mutex);
		for (const auto& uptr : m_resources.items()) {
			if (uptr->current_state.load(std::memory_order_acquire) == state::queued) {
				uptr->current_state.store(state::loading, std::memory_order_release);

				const id rid = uptr->resource.read() ? uptr->resource.read()->id() : m_path_to_id[uptr->path];

				ids_to_load.push_back(rid);
			}
		}
	}

	for (const id rid : ids_to_load) {
		auto t = launch_load(rid);
		t.start();
		m_in_flight.push_back(std::move(t));
	}
}

template <typename R>
auto gse::resource::loader<R>::launch_load(const id rid) -> async::task<> {
	co_await async::yield_to_worker();

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
			co_return;
		}
	}

	if (m_pre_load_fn && !path.empty()) {
		m_pre_load_fn(path);
	}

	assert(m_data.channels != nullptr, "asset::registry::run must run before flush()");
	asset::load_ctx ctx{
		.assets = m_data,
		.channels = *m_data.channels
	};

	try {
		co_await resource_ptr->load(ctx);
	}
	catch (...) {
		update_state(rid, state::failed);
		co_return;
	}

	update_state(rid, state::loaded);
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
	reap_done_tasks();

	std::vector<id> reloads_to_process;
	{
		std::lock_guard lock(m_reload_mutex);
		reloads_to_process.swap(m_pending_reloads);
	}

	if (reloads_to_process.empty()) {
		return;
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

		auto t = launch_reload(rid);
		t.start();
		m_in_flight.push_back(std::move(t));
	}
}

template <typename R>
auto gse::resource::loader<R>::launch_reload(const id rid) -> async::task<> {
	co_await async::yield_to_worker();

	resource_slot<R>* s;
	std::filesystem::path path;
	{
		std::lock_guard lock(m_mutex);
		s = slot_ptr(rid);
		if (!s) {
			co_return;
		}
		path = s->path;
	}

	auto new_resource = std::make_unique<R>(path);

	assert(m_data.channels != nullptr, "asset::registry::run must run before finalize_reloads()");
	asset::load_ctx ctx{
		.assets = m_data,
		.channels = *m_data.channels
	};

	try {
		co_await new_resource->load(ctx);
	}
	catch (...) {
		s->current_state.store(state::failed, std::memory_order_release);
		co_return;
	}

	if (auto old_resource = s->resource.take_ready()) {
		old_resource->unload();
	}

	s->resource.write() = std::move(new_resource);
	s->resource.publish();
	s->version.fetch_add(1, std::memory_order_release);
	s->current_state.store(state::loaded, std::memory_order_release);

	log::println(log::category::assets, "Hot reload reloaded resource: {}", path.filename().string());
}

template <typename R>
auto gse::resource::loader<R>::get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	assert(s, "Resource with ID {} not found in this loader.", id);
	return handle<R>(id, s, s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(resource_id);
	assert(s, "Resource with ID {} not found in this loader.", resource_id);
	return handle<R>(resource_id, s, s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::try_get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	if (!s) {
		return handle<R>{};
	}
	return handle<R>(id, s, s->version.load(std::memory_order_acquire));
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
	return handle<R>(resource_id, s, s->version.load(std::memory_order_acquire));
}

template <typename R>
auto gse::resource::loader<R>::enqueue(const std::string& name, std::unique_ptr<R> resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			const auto* s = slot_ptr(resource_id);
			return handle<R>(resource_id, s, s->version.load(std::memory_order_acquire));
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
