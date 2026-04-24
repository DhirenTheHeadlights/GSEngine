export module gse.assets:resource_loader;

import std;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.assert;

import :resource_handle;

namespace gse {
	template<typename Resource, typename Context>
	concept is_resource =
		requires(Resource res, Context & ctx) {
			{ res.load(ctx) } -> std::same_as<void>;
			{ res.unload() } -> std::same_as<void>;
	};

	template <typename C>
	concept resource_context = requires(C& ctx, const C& cctx, std::type_index ti, id rid) {
		{ cctx.gpu_queue_size() } -> std::convertible_to<std::size_t>;
		{ ctx.mark_pending_for_finalization(ti, rid) } -> std::same_as<void>;
		{ ctx.wait_idle() } -> std::same_as<void>;
		{ ctx.process_gpu_queue() } -> std::same_as<void>;
	};
}

export namespace gse::resource {
	class loader_base;

	class gpu_work_token final : public non_copyable {
	public:
		gpu_work_token(loader_base* loader, id resource_id, std::size_t queue_size_before);
		~gpu_work_token() override;

		auto queue_size_before() const -> size_t;
	private:
		loader_base* m_loader;
		id m_id;
		std::size_t m_queue_size_before;
	};

	class reload_token final : public non_copyable {
	public:
		reload_token(loader_base* loader, id resource_id, std::size_t queue_size_before);
		~reload_token() override;
	private:
		loader_base* m_loader;
		id m_id;
		std::size_t m_queue_size_before;
	};

	class loader_base {
	public:
		virtual ~loader_base() = default;
		virtual auto flush() -> void = 0;
		virtual auto update_state(id resource_id, state new_state) -> void = 0;
		virtual auto mark_for_gpu_finalization(id resource_id) -> void = 0;
		virtual auto finalize_state(id resource_id, size_t queue_size_before) -> void = 0;
		virtual auto queue_reload_by_path(const std::filesystem::path& baked_path) -> void = 0;
		virtual auto queue_by_path(const std::filesystem::path& baked_path) -> void = 0;
		virtual auto finalize_reloads() -> void = 0;
	};

	template <typename Resource, typename RenderingContext>
		requires gse::is_resource<Resource, RenderingContext> && gse::resource_context<RenderingContext>
	class loader final : public loader_base, public non_copyable {
	public:
		explicit loader(RenderingContext& context) : m_context(context) {}
		~loader() override = default;

		auto flush() -> void override;

		auto update_state(id resource_id, state new_state) -> void override;
		auto mark_for_gpu_finalization(id resource_id) -> void override;
		auto finalize_state(id resource_id, size_t queue_size_before) -> void override;

		auto queue_reload(id resource_id) -> void;
		auto queue_reload_by_path(const std::filesystem::path& baked_path) -> void override;
		auto queue_by_path(const std::filesystem::path& baked_path) -> void override;
		auto finalize_reloads() -> void override;

		auto set_pre_load_fn(std::function<void(const std::filesystem::path&)> fn) -> void { m_pre_load_fn = std::move(fn); }

		auto get(id id) const -> handle<Resource>;
		auto get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto try_get(id id) const -> handle<Resource>;
		auto try_get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto instantly_load(id resource_id) -> void;

		[[nodiscard]] auto state_of(id resource_id) const -> state;

		template <typename... Args>
			requires std::constructible_from<Resource, std::string, Args...>
		auto queue(const std::string& name, Args&&... args) -> handle<Resource>;

		auto add(Resource&& resource) -> handle<Resource>;
	private:
		RenderingContext& m_context;
		id_mapped_collection<std::unique_ptr<resource_slot<Resource>>> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		task::group m_load_group{ generate_id("resource.loader.load") };
		mutable std::mutex m_mutex;

		std::vector<id> m_pending_reloads;
		std::mutex m_reload_mutex;

		std::function<void(const std::filesystem::path&)> m_pre_load_fn;

		auto slot_ptr(this auto&& self, id id) -> resource_slot<Resource>*;
	};
}

gse::resource::gpu_work_token::gpu_work_token(loader_base* loader, const id resource_id, const std::size_t queue_size_before)
	: m_loader(loader), m_id(resource_id), m_queue_size_before(queue_size_before) {
	m_loader->update_state(m_id, state::loading);
}

gse::resource::gpu_work_token::~gpu_work_token() {
	if (std::uncaught_exceptions()) {
		m_loader->update_state(m_id, state::failed);
		return;
	}
	m_loader->finalize_state(m_id, m_queue_size_before);
}

auto gse::resource::gpu_work_token::queue_size_before() const -> size_t {
	return m_queue_size_before;
}

gse::resource::reload_token::reload_token(loader_base* loader, const id resource_id, const std::size_t queue_size_before)
	: m_loader(loader), m_id(resource_id), m_queue_size_before(queue_size_before) {
	m_loader->update_state(m_id, state::reloading);
}

gse::resource::reload_token::~reload_token() {
	if (std::uncaught_exceptions()) {
		m_loader->update_state(m_id, state::loaded);
		return;
	}
	m_loader->finalize_state(m_id, m_queue_size_before);
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::state_of(id resource_id) const -> state {
	std::lock_guard lock(m_mutex);
	if (const auto* s = slot_ptr(resource_id)) {
		return s->current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::slot_ptr(this auto&& self, id id) -> resource_slot<R>* {
	if (auto* uptr = self.m_resources.try_get(id)) {
		return uptr->get();
	}
	return nullptr;
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::flush() -> void {
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

	std::vector<std::function<void()>> jobs;
	jobs.reserve(ids_to_load.size());

	for (const id rid : ids_to_load) {
		jobs.emplace_back([this, rid] {
			gpu_work_token token(this, rid, m_context.gpu_queue_size());

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
				} else {
					update_state(rid, state::failed);
					return;
				}
			}

			if (m_pre_load_fn && !path.empty()) {
				m_pre_load_fn(path);
			}

			resource_ptr->load(m_context);
		});
	}

	m_load_group.post_range(jobs.begin(), jobs.end(), generate_id("resource.load"));
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::update_state(id resource_id, state new_state) -> void {
	std::lock_guard lock(m_mutex);
	if (auto* s = slot_ptr(resource_id)) {
		s->current_state.store(new_state, std::memory_order_release);
	}
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::mark_for_gpu_finalization(id resource_id) -> void {
	m_context.mark_pending_for_finalization(typeid(R), resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::finalize_state(id resource_id, size_t queue_size_before) -> void {
	if (m_context.gpu_queue_size() > queue_size_before) {
		m_context.mark_pending_for_finalization(typeid(R), resource_id);
	}
	else {
		update_state(resource_id, state::loaded);
	}
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::queue_reload(id resource_id) -> void {
	std::lock_guard lock(m_reload_mutex);

	if (std::ranges::find(m_pending_reloads, resource_id) != m_pending_reloads.end()) {
		return;
	}

	m_pending_reloads.push_back(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::queue_reload_by_path(const std::filesystem::path& baked_path) -> void {
	std::lock_guard lock(m_mutex);

	auto it = m_path_to_id.find(baked_path);
	if (it == m_path_to_id.end()) {
		return;
	}

	queue_reload(it->second);
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::queue_by_path(const std::filesystem::path& baked_path) -> void {
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

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::finalize_reloads() -> void {
	std::vector<id> reloads_to_process;
	{
		std::lock_guard lock(m_reload_mutex);
		reloads_to_process.swap(m_pending_reloads);
	}

	if (reloads_to_process.empty()) {
		return;
	}

	m_context.wait_idle();

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
		const auto queue_size_before = m_context.gpu_queue_size();
		new_resource->load(m_context);
		const bool queued_gpu_work = m_context.gpu_queue_size() > queue_size_before;

		if (queued_gpu_work) {
			m_context.process_gpu_queue();
			m_context.wait_idle();
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

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::get(id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	assert(s, std::source_location::current(), "Resource with ID {} not found in this loader.", id);
	return handle<R>(id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(resource_id);
	assert(s, std::source_location::current(), "Resource with ID {} not found in this loader.", resource_id);
	return handle<R>(resource_id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::try_get(id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto* s = slot_ptr(id);
	if (!s) {
		return handle<R>{};
	}
	return handle<R>(id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::try_get(const std::string& filename_no_ext) const -> handle<R> {
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

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::instantly_load(id resource_id) -> void {
	resource_slot<R>* s;
	{
		std::lock_guard lock(m_mutex);
		s = slot_ptr(resource_id);
		assert(s, std::source_location::current(), "invalid id");
		if (s->current_state == state::loaded)
			return;
	}

	while (s->current_state.load(std::memory_order_acquire) == state::loading) {
		std::this_thread::yield();
	}

	if (s->current_state.load(std::memory_order_acquire) == state::loaded) {
		return;
	}

	const gpu_work_token token(this, resource_id, m_context.gpu_queue_size());

	if (m_pre_load_fn && !s->path.empty()) {
		m_pre_load_fn(s->path);
	}

	const auto queue_before = m_context.gpu_queue_size();

	if (!s->resource.read()) {
		s->resource.write() = std::make_unique<R>(s->path);
		s->resource.publish();
	}
	const_cast<R*>(s->resource.read().get())->load(m_context);

	const bool work_was_queued = m_context.gpu_queue_size() > queue_before;

	if (work_was_queued) {
		m_context.process_gpu_queue();
		m_context.wait_idle();
	}
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
template <typename... Args> requires std::constructible_from<R, std::string, Args...>
auto gse::resource::loader<R, C>::queue(const std::string& name, Args&&... args) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			const auto* s = slot_ptr(resource_id);
			return handle<R>(resource_id, const_cast<resource_slot<R>*>(s), s->version.load(std::memory_order_acquire));
		}
	}

	auto temp_resource = std::make_unique<R>(name, std::forward<Args>(args)...);
	const auto resource_id = temp_resource->id();

	auto slot = std::make_unique<resource_slot<R>>(std::move(temp_resource), state::queued, "");
	auto* slot_raw = slot.get();
	m_resources.add(resource_id, std::move(slot));
	return handle<R>(resource_id, slot_raw, 0);
}

template <typename R, typename C> requires gse::is_resource<R, C> && gse::resource_context<C>
auto gse::resource::loader<R, C>::add(R&& resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto id = resource.id();
	assert(!m_resources.contains(id), std::source_location::current(), "Resource with ID {} already exists.", id);

	auto resource_ptr = std::make_unique<R>(std::move(resource));
	auto slot = std::make_unique<resource_slot<R>>(std::move(resource_ptr), state::loaded, "");
	auto* slot_raw = slot.get();
	m_resources.add(id, std::move(slot));

	return handle<R>(id, slot_raw, 0);
}
