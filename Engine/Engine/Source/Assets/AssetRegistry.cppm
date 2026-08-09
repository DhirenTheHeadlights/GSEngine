export module gse.assets:registry;

import std;

import :resource_loader;
import :resource_handle;
import :asset_state;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.fs;

export namespace gse::asset {
	template <typename T>
	auto add_loader(
		data& d
	) -> void;

	template <typename T>
	auto set_pre_load_fn(
		data& d,
		std::function<asset_result(const std::filesystem::path&)> fn
	) -> void;

	template <typename T>
	[[nodiscard]]
	auto register_by_path(
		const data& d,
		const std::filesystem::path& baked_path
	) -> asset_result;

	template <typename T>
	auto get(
		const data& d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto get(
		shared_view<data> d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto get(
		const data& d,
		const std::string& filename
	) -> resource::handle<T>;

	template <typename T>
	auto get(
		shared_view<data> d,
		const std::string& filename
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		const data& d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		shared_view<data> d,
		id resource_id
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		const data& d,
		const std::string& filename
	) -> resource::handle<T>;

	template <typename T>
	auto try_get(
		shared_view<data> d,
		const std::string& filename
	) -> resource::handle<T>;

	template <typename T, typename... Args>
	auto queue(
		const data& d,
		const std::string& name,
		Args&&... args
	) -> resource::handle<T>;

	template <typename T, typename... Args>
	auto queue(
		shared_view<data> d,
		const std::string& name,
		Args&&... args
	) -> resource::handle<T>;

	template <typename T>
	auto add(
		const data& d,
		T&& resource
	) -> resource::handle<T>;

	template <typename T>
	[[nodiscard]]
	auto resource_state(
		const data& d,
		id resource_id
	) -> resource::state;

	template <typename T>
	[[nodiscard]]
	auto resource_state(
		shared_view<data> d,
		id resource_id
	) -> resource::state;
}

namespace gse::resource {
	template <typename Resource>
	class loader;
}

namespace gse::asset {
	template <typename T>
	auto loader_for(
		const data& d
	) -> resource::loader<T>*;

	template <typename T>
	auto loader_for(
		shared_view<data> d
	) -> resource::loader<T>*;

	auto loader_base_for(
		const data& d,
		id type_index
	) -> resource::loader_base*;

	auto loader_base_for(
		shared_view<data> d,
		id type_index
	) -> resource::loader_base*;
}

namespace gse::resource {
	template <typename Resource>
	class loader final : public loader_base, public non_copyable {
	public:
		explicit loader(
			asset::data& d
		);

		~loader() = default;

		auto flush() -> void override;

		[[nodiscard]]
		auto register_by_path(
			const std::filesystem::path& baked_path
		) -> asset_result;

		auto finalize_reloads() -> void override;

		auto set_pre_load_fn(
			std::function<asset_result(const std::filesystem::path&)> fn
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
		id_mapped_collection<std::shared_ptr<resource_slot<Resource>>> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		std::vector<async::task<>> m_in_flight;
		mutable std::mutex m_mutex;

		std::function<asset_result(const std::filesystem::path&)> m_pre_load_fn;

		auto slot_ptr(
			this auto&& self,
			id id
		);

		auto request_load(
			const std::shared_ptr<resource_slot<Resource>>& slot
		) const -> void;

		auto launch_load(
			id rid
		) -> async::task<>;

		auto launch_reload(
			id rid,
			std::uint64_t revision
		) -> async::task<>;

		auto reap_done_tasks() -> void;
	};
}

template <typename T>
auto gse::asset::add_loader(data& d) -> void {
	const auto type_id = id_of<T>();
	assert(!d.resource_loaders.contains(type_id), "Resource loader for type {} already exists.", type_tag<T>());

	d.resource_loaders[type_id] = std::make_unique<resource::loader<T>>(d);
}

template <typename T>
auto gse::asset::set_pre_load_fn(data& d, std::function<asset_result(const std::filesystem::path&)> fn) -> void {
	loader_for<T>(d)->set_pre_load_fn(std::move(fn));
}

template <typename T>
auto gse::asset::register_by_path(const data& d, const std::filesystem::path& baked_path) -> asset_result {
	return loader_for<T>(d)->register_by_path(baked_path);
}

template <typename T>
auto gse::asset::get(const data& d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->get(resource_id);
}

template <typename T>
auto gse::asset::get(const shared_view<data> d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->get(resource_id);
}

template <typename T>
auto gse::asset::get(const data& d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->get(filename);
}

template <typename T>
auto gse::asset::get(const shared_view<data> d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->get(filename);
}

template <typename T>
auto gse::asset::try_get(const data& d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(resource_id);
}

template <typename T>
auto gse::asset::try_get(const shared_view<data> d, const id resource_id) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(resource_id);
}

template <typename T>
auto gse::asset::try_get(const data& d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(filename);
}

template <typename T>
auto gse::asset::try_get(const shared_view<data> d, const std::string& filename) -> resource::handle<T> {
	return loader_for<T>(d)->try_get(filename);
}

template <typename T, typename... Args>
auto gse::asset::queue(const data& d, const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader_for<T>(d)->enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
}

template <typename T, typename... Args>
auto gse::asset::queue(const shared_view<data> d, const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader_for<T>(d)->enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
}

template <typename T>
auto gse::asset::add(const data& d, T&& resource) -> resource::handle<T> {
	return loader_for<T>(d)->add(std::make_unique<T>(std::forward<T>(resource)));
}

template <typename T>
auto gse::asset::resource_state(const data& d, const id resource_id) -> resource::state {
	return loader_for<T>(d)->state_of(resource_id);
}

template <typename T>
auto gse::asset::resource_state(const shared_view<data> d, const id resource_id) -> resource::state {
	return loader_for<T>(d)->state_of(resource_id);
}

template <typename T>
auto gse::asset::loader_for(const data& d) -> resource::loader<T>* {
	return static_cast<resource::loader<T>*>(loader_base_for(d, id_of<T>()));
}

template <typename T>
auto gse::asset::loader_for(const shared_view<data> d) -> resource::loader<T>* {
	return static_cast<resource::loader<T>*>(loader_base_for(d, id_of<T>()));
}

auto gse::asset::loader_base_for(const data& d, const id type_id) -> resource::loader_base* {
	assert(d.resource_loaders.contains(type_id), "Resource loader for id {} does not exist.", type_id.number());
	return d.resource_loaders.at(type_id).get();
}

auto gse::asset::loader_base_for(const shared_view<data> d, const id type_id) -> resource::loader_base* {
	assert(d.resource_loaders.contains(type_id), "Resource loader for id {} does not exist.", type_id.number());
	return d.resource_loaders.at(type_id).get();
}

template <typename R>
gse::resource::loader<R>::loader(asset::data& d) : m_data(d) {}

template <typename R>
auto gse::resource::loader<R>::set_pre_load_fn(std::function<asset_result(const std::filesystem::path&)> fn) -> void {
	m_pre_load_fn = std::move(fn);
}

template <typename R>
auto gse::resource::loader<R>::state_of(const id resource_id) const -> state {
	std::lock_guard lock(m_mutex);
	if (const auto s = slot_ptr(resource_id)) {
		return s->current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename R>
auto gse::resource::loader<R>::slot_ptr(this auto&& self, const id id) {
	auto* slot = self.m_resources.try_get(id);
	return slot ? *slot : nullptr;
}

template <typename R>
auto gse::resource::loader<R>::reap_done_tasks() -> void {
	std::erase_if(
		m_in_flight,
		[](const async::task<>& t) {
			return t.done();
		}
	);
}

template <typename R>
auto gse::resource::loader<R>::flush() -> void {
	reap_done_tasks();

	std::vector<id> ids_to_load;
	{
		std::lock_guard lock(m_mutex);
		for (const auto& slot : m_resources.items()) {
			if (slot->current_state.load(std::memory_order_acquire) == state::queued && !slot->load_in_flight) {
				slot->load_in_flight = true;
				slot->current_state.store(state::loading, std::memory_order_release);
				ids_to_load.push_back(slot->pending_resource->id());
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

	std::shared_ptr<resource_slot<R>> slot;
	std::unique_ptr<R> candidate;
	std::filesystem::path path;
	{
		std::lock_guard lock(m_mutex);
		slot = slot_ptr(rid);
		if (!slot) {
			co_return;
		}
		path = slot->path;
		candidate = std::move(slot->pending_resource);
		assert(candidate != nullptr, "Queued resource has no load candidate: {}", path.generic_display_string());
	}

	auto fail = [this, slot, path](asset_error error) {
		log::println(
			log::level::error,
			log::category::assets,
			"Unable to load {}: {}",
			path.generic_display_string(),
			error.detail
		);
		std::lock_guard lock(m_mutex);
		slot->error.store(std::make_shared<const asset_error>(std::move(error)), std::memory_order_release);
		if (!slot->current.load(std::memory_order_acquire) && slot->requested_reload > slot->started_reload) {
			slot->started_reload = slot->requested_reload;
			slot->pending_resource = std::make_unique<R>(path);
			slot->current_state.store(state::queued, std::memory_order_release);
		}
		else {
			slot->current_state.store(slot->current.load(std::memory_order_acquire) ? state::loaded : state::failed, std::memory_order_release);
		}
		slot->load_in_flight = false;
	};

	if (m_pre_load_fn && !path.empty()) {
		asset_result prepared{};
		try {
			prepared = m_pre_load_fn(path);
		}
		catch (const std::exception& exception) {
			prepared = std::unexpected(asset_error{
				.code = asset_error_code::load_failure,
				.path = path,
				.detail = exception.what()
			});
		}
		catch (...) {
			prepared = std::unexpected(asset_error{
				.code = asset_error_code::load_failure,
				.path = path,
				.detail = "Resource preparation failed"
			});
		}
		if (!prepared) {
			fail(std::move(prepared.error()));
			co_return;
		}
	}

	assert(m_data.channels.bound(), "asset::run must run before flush()");
	asset::load_ctx ctx{
		.assets = m_data,
		.channels = m_data.channels
	};

	asset_result loaded{};
	try {
		loaded = co_await candidate->load(ctx);
	}
	catch (const std::exception& exception) {
		loaded = std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = exception.what()
		});
	}
	catch (...) {
		loaded = std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = "Resource load threw an unknown exception"
		});
	}
	if (!loaded) {
		fail(std::move(loaded.error()));
		co_return;
	}

	try {
		auto immutable = std::shared_ptr<const R>(std::move(candidate));
		std::lock_guard lock(m_mutex);
		const auto version = slot->next_version++;
		slot->current.store(
			std::make_shared<const generation<R>>(
				generation<R>{
					.resource = std::move(immutable),
					.version = version
				}
			),
			std::memory_order_release
		);
		slot->error.store(std::shared_ptr<const asset_error>{}, std::memory_order_release);
		slot->current_state.store(state::loaded, std::memory_order_release);
		slot->load_in_flight = false;
	}
	catch (const std::exception& exception) {
		fail(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = exception.what()
		});
		co_return;
	}
}

template <typename R>
auto gse::resource::loader<R>::register_by_path(const std::filesystem::path& baked_path) -> asset_result {
	std::lock_guard lock(m_mutex);

	if (const auto found = m_path_to_id.find(baked_path); found != m_path_to_id.end()) {
		if (auto slot = slot_ptr(found->second)) {
			if (slot->current.load(std::memory_order_acquire)) {
				++slot->requested_reload;
			}
			else if (slot->load_in_flight) {
				++slot->requested_reload;
			}
			else if (slot->current_state.load(std::memory_order_acquire) == state::failed) {
				slot->pending_resource = std::make_unique<R>(slot->path);
				slot->current_state.store(state::queued, std::memory_order_release);
				slot->error.store(std::shared_ptr<const asset_error>{}, std::memory_order_release);
			}
		}
		return {};
	}

	std::unique_ptr<R> temp_resource;
	try {
		temp_resource = std::make_unique<R>(baked_path);
		const id resource_id = temp_resource->id();

		auto slot = std::make_shared<resource_slot<R>>(std::move(temp_resource), state::unloaded, baked_path);
		if (!m_resources.add(resource_id, slot)) {
			return std::unexpected(asset_error{
				.code = asset_error_code::ambiguous_source,
				.path = baked_path,
				.detail = "Resource ID collision while cataloging baked asset"
			});
		}
		m_path_to_id[baked_path] = resource_id;
	}
	catch (const std::exception& exception) {
		return std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = baked_path,
			.detail = exception.what()
		});
	}
	return {};
}

template <typename R>
auto gse::resource::loader<R>::finalize_reloads() -> void {
	reap_done_tasks();

	std::vector<std::pair<id, std::uint64_t>> reloads_to_process;
	{
		std::lock_guard lock(m_mutex);
		for (const auto& slot : m_resources.items()) {
			if (!slot->current.load(std::memory_order_acquire) || slot->load_in_flight || slot->requested_reload <= slot->started_reload) {
				continue;
			}
			slot->started_reload = slot->requested_reload;
			slot->load_in_flight = true;
			slot->current_state.store(state::reloading, std::memory_order_release);
			const auto current = slot->current.load(std::memory_order_acquire);
			reloads_to_process.emplace_back(current->resource->id(), slot->started_reload);
		}
	}

	for (const auto [rid, revision] : reloads_to_process) {
		auto t = launch_reload(rid, revision);
		t.start();
		m_in_flight.push_back(std::move(t));
	}
}

template <typename R>
auto gse::resource::loader<R>::launch_reload(const id rid, const std::uint64_t revision) -> async::task<> {
	co_await async::yield_to_worker();

	std::shared_ptr<resource_slot<R>> slot;
	std::filesystem::path path;
	{
		std::lock_guard lock(m_mutex);
		slot = slot_ptr(rid);
		if (!slot) {
			co_return;
		}
		path = slot->path;
	}

	auto fail = [this, slot](asset_error error) {
		std::lock_guard lock(m_mutex);
		slot->error.store(std::make_shared<const asset_error>(std::move(error)), std::memory_order_release);
		slot->current_state.store(slot->current.load(std::memory_order_acquire) ? state::loaded : state::failed, std::memory_order_release);
		slot->load_in_flight = false;
	};
	std::unique_ptr<R> new_resource;
	try {
		new_resource = std::make_unique<R>(path);
	}
	catch (const std::exception& exception) {
		fail(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = exception.what()
		});
		co_return;
	}

	assert(m_data.channels.bound(), "asset::run must run before finalize_reloads()");
	asset::load_ctx ctx{
		.assets = m_data,
		.channels = m_data.channels
	};

	asset_result loaded{};
	try {
		loaded = co_await new_resource->load(ctx);
	}
	catch (const std::exception& exception) {
		loaded = std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = exception.what()
		});
	}
	catch (...) {
		loaded = std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = "Resource reload threw an unknown exception"
		});
	}
	if (!loaded) {
		fail(std::move(loaded.error()));
		co_return;
	}

	try {
		auto immutable = std::shared_ptr<const R>(std::move(new_resource));
		std::lock_guard lock(m_mutex);
		const auto version = slot->next_version++;
		slot->current.store(
			std::make_shared<const generation<R>>(
				generation<R>{
					.resource = std::move(immutable),
					.version = version
				}
			),
			std::memory_order_release
		);
		slot->error.store(std::shared_ptr<const asset_error>{}, std::memory_order_release);
		slot->current_state.store(state::loaded, std::memory_order_release);
		slot->load_in_flight = false;
		assert(slot->started_reload == revision, "Resource reload revision changed while in flight");
	}
	catch (const std::exception& exception) {
		fail(asset_error{
			.code = asset_error_code::load_failure,
			.path = path,
			.detail = exception.what()
		});
		co_return;
	}

	log::println(log::category::assets, "Hot reload reloaded resource: {}", path.filename().generic_display_string());
}

template <typename R>
auto gse::resource::loader<R>::get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto s = slot_ptr(id);
	assert(s != nullptr, "Resource with ID {} not found in this loader.", id);
	request_load(s);
	return handle<R>(id, s);
}

template <typename R>
auto gse::resource::loader<R>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto s = slot_ptr(resource_id);
	assert(s != nullptr, "Resource with ID {} not found in this loader.", resource_id);
	request_load(s);
	return handle<R>(resource_id, s);
}

template <typename R>
auto gse::resource::loader<R>::try_get(const id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto s = slot_ptr(id);
	if (!s) {
		return handle<R>{};
	}
	request_load(s);
	return handle<R>(id, s);
}

template <typename R>
auto gse::resource::loader<R>::try_get(const std::string& filename_no_ext) const -> handle<R> {
	if (!gse::exists(filename_no_ext)) {
		return handle<R>{};
	}
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto s = slot_ptr(resource_id);
	if (!s) {
		return handle<R>{};
	}
	request_load(s);
	return handle<R>(resource_id, s);
}

template <typename R>
auto gse::resource::loader<R>::request_load(const std::shared_ptr<resource_slot<R>>& slot) const -> void {
	auto expected = state::unloaded;
	slot->current_state.compare_exchange_strong(expected, state::queued, std::memory_order_acq_rel, std::memory_order_relaxed);
}

template <typename R>
auto gse::resource::loader<R>::enqueue(const std::string& name, std::unique_ptr<R> resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			return handle<R>(resource_id, slot_ptr(resource_id));
		}
	}

	const auto resource_id = resource->id();

	auto slot = std::make_shared<resource_slot<R>>(std::move(resource), state::queued, "");
	assert(m_resources.add(resource_id, slot), "Resource with ID {} already exists.", resource_id);
	return handle<R>(resource_id, std::move(slot));
}

template <typename R>
auto gse::resource::loader<R>::add(std::unique_ptr<R> resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto id = resource->id();
	assert(!m_resources.contains(id), "Resource with ID {} already exists.", id);

	auto slot = std::make_shared<resource_slot<R>>(std::move(resource), state::loaded, "");
	assert(m_resources.add(id, slot), "Resource with ID {} already exists.", id);

	return handle<R>(id, std::move(slot));
}
