export module gse.platform:resource_loader;

import std;

import gse.utility;
import gse.assert;

namespace gse {
	template<typename Resource, typename Context>
	concept is_resource =
		requires(Resource res, Context & ctx) {
			{ res.load(ctx) } -> std::same_as<void>;
			{ res.unload() } -> std::same_as<void>;
	};
}

export namespace gse::resource {
	class loader_base;

	enum struct state {
		unloaded,
		queued,
		loading,
		loaded,
		reloading,
		failed
	};

	template <typename Resource>
	class handle : identifiable_owned {
	public:
		handle() = default;
		handle(
			id resource_id,
			loader_base* loader
		);
		handle(
			id resource_id,
			loader_base* loader,
			std::uint32_t version
		);

		[[nodiscard]] auto resolve(
		) const -> Resource*;

		[[nodiscard]] auto state(
		) const -> state;

		[[nodiscard]] auto valid(
		) const -> bool;

		[[nodiscard]] auto id(
		) const -> id;

		[[nodiscard]] auto version(
		) const -> std::uint32_t;

		[[nodiscard]] auto is_current(
		) const -> bool;

		[[nodiscard]] auto operator->(
		) const -> Resource*;

		[[nodiscard]] auto operator*(
		) const -> Resource&;

		[[nodiscard]] auto operator==(
			const handle& other
		) const -> bool;

		[[nodiscard]] auto operator!=(
			const handle& other
		) const -> bool;

		explicit operator bool(
		) const;
	private:
		loader_base* m_loader = nullptr;
		mutable std::uint32_t m_version = 0;
	};
}

export namespace gse::resource {
	class loader_base {
	public:
		virtual ~loader_base() = default;
		virtual auto flush() -> void = 0;
		[[nodiscard]] virtual auto resource(id resource_id) -> void* = 0;
		[[nodiscard]] virtual auto resource_state(id resource_id) const -> state = 0;
		[[nodiscard]] virtual auto resource_version(id resource_id) const -> std::uint32_t = 0;
		virtual auto update_state(id resource_id, state new_state) -> void = 0;
		virtual auto mark_for_gpu_finalization(id resource_id) -> void = 0;
		virtual auto finalize_state(id resource_id, size_t queue_size_before) -> void = 0;
		virtual auto queue_reload_by_path(const std::filesystem::path& baked_path) -> void = 0;
		virtual auto queue_by_path(const std::filesystem::path& baked_path) -> void = 0;
		virtual auto finalize_reloads() -> void = 0;
	};

	class gpu_work_token final : public non_copyable {
	public:
		gpu_work_token(loader_base* loader, const id resource_id, const std::size_t queue_size_before)
			: m_loader(loader), m_id(resource_id), m_queue_size_before(queue_size_before) {
			m_loader->update_state(m_id, state::loading);
		}

		~gpu_work_token() override {
			if (std::uncaught_exceptions()) {
				m_loader->update_state(m_id, state::failed);
				return;
			}
			m_loader->finalize_state(m_id, m_queue_size_before);
		}

		auto queue_size_before() const -> size_t {
			return m_queue_size_before;
		}
	private:
		loader_base* m_loader;
		id m_id;
		std::size_t m_queue_size_before;
	};

	class reload_token final : public non_copyable {
	public:
		reload_token(loader_base* loader, const id resource_id, const std::size_t queue_size_before)
			: m_loader(loader), m_id(resource_id), m_queue_size_before(queue_size_before) {
			m_loader->update_state(m_id, state::reloading);
		}

		~reload_token() override {
			if (std::uncaught_exceptions()) {
				m_loader->update_state(m_id, state::loaded);
				return;
			}
			m_loader->finalize_state(m_id, m_queue_size_before);
		}
	private:
		loader_base* m_loader;
		id m_id;
		std::size_t m_queue_size_before;
	};

	template <typename Resource, typename RenderingContext>
		requires gse::is_resource<Resource, RenderingContext>
	class loader final : public loader_base, public non_copyable {
	public:
		struct slot {
			double_buffer<std::unique_ptr<Resource>> resource;
			std::atomic<state> current_state;
			std::filesystem::path path;
			std::atomic<std::uint32_t> version{0};

			slot(std::unique_ptr<Resource>&& res, const state s, const std::filesystem::path& p)
				: current_state(s), path(p) {
				resource.write() = std::move(res);
				resource.publish();
			}

			slot(slot&& other) noexcept
				: current_state(other.current_state.load(std::memory_order_relaxed)),
				path(std::move(other.path)),
				version(other.version.load(std::memory_order_relaxed)) {
				resource.write() = std::move(const_cast<std::unique_ptr<Resource>&>(other.resource.read()));
				resource.publish();
			}

			auto operator=(slot&& other) noexcept -> slot& {
				if (this != &other) {
					resource.write() = std::move(const_cast<std::unique_ptr<Resource>&>(other.resource.read()));
					resource.publish();
					current_state.store(other.current_state.load(std::memory_order_relaxed));
					path = std::move(other.path);
					version.store(other.version.load(std::memory_order_relaxed));
				}
				return *this;
			}

			slot(const slot&) = delete;
			auto operator=(const slot&) -> slot& = delete;
		};

		explicit loader(RenderingContext& context) : m_context(context) {}
		~loader() override = default;

		auto flush() -> void override;

		[[nodiscard]] auto resource(id resource_id) -> void* override;
		[[nodiscard]] auto resource_state(id resource_id) const -> state override;
		[[nodiscard]] auto resource_version(id resource_id) const -> std::uint32_t override;

		auto update_state(id resource_id, state new_state) -> void override;
		auto mark_for_gpu_finalization(id resource_id) -> void override;
		auto finalize_state(id resource_id, size_t queue_size_before) -> void override;

		auto queue_reload(id resource_id) -> void;
		auto queue_reload_by_path(const std::filesystem::path& baked_path) -> void override;
		auto queue_by_path(const std::filesystem::path& baked_path) -> void override;
		auto finalize_reloads() -> void override;

		auto get(id id) const -> handle<Resource>;
		auto get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto try_get(id id) const -> handle<Resource>;
		auto try_get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto instantly_load(id resource_id) -> void;

		template <typename... Args>
			requires std::constructible_from<Resource, std::string, Args...>
		auto queue(const std::string& name, Args&&... args) -> handle<Resource>;

		auto add(Resource&& resource) -> handle<Resource>;
	private:
		RenderingContext& m_context;
		id_mapped_collection<slot> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		task::group m_load_group{ generate_id("resource.loader.load") };
		mutable std::mutex m_mutex;

		std::vector<id> m_pending_reloads;
		std::mutex m_reload_mutex;

		auto get_unlocked(id id) const -> handle<Resource>;
		auto try_get_unlocked(id id) const -> handle<Resource>;
		auto resource_version_unlocked(id resource_id) const -> std::uint32_t;
	};
}

template <typename Resource>
gse::resource::handle<Resource>::handle(const gse::id resource_id, loader_base* loader) : identifiable_owned(resource_id), m_loader(loader) {
	if (m_loader) {
		m_version = m_loader->resource_version(resource_id);
	}
}

template <typename Resource>
gse::resource::handle<Resource>::handle(const gse::id resource_id, loader_base* loader, const std::uint32_t version) : identifiable_owned(resource_id), m_loader(loader), m_version(version) {}

template <typename Resource>
auto gse::resource::handle<Resource>::resolve() const -> Resource* {
	if (!m_loader) return nullptr;
	m_version = m_loader->resource_version(owner_id());
	return static_cast<Resource*>(m_loader->resource(owner_id()));
}

template <typename Resource>
auto gse::resource::handle<Resource>::state() const -> resource::state {
	if (!m_loader) return state::unloaded;
	return m_loader->resource_state(owner_id());
}

template <typename Resource>
auto gse::resource::handle<Resource>::valid() const -> bool {
	const auto s = state();
	return m_loader && (s == state::loaded || s == state::reloading);
}

template <typename Resource>
auto gse::resource::handle<Resource>::id() const -> gse::id {
	return owner_id();
}

template <typename Resource>
auto gse::resource::handle<Resource>::version() const -> std::uint32_t {
	return m_version;
}

template <typename Resource>
auto gse::resource::handle<Resource>::is_current() const -> bool {
	if (!m_loader) return false;
	return m_version == m_loader->resource_version(owner_id());
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator->() const -> Resource* {
	Resource* resource = resolve();
	assert(resource, std::source_location::current(), "Attempting to access an unloaded or invalid resource with ID: {}", owner_id());
	return resource;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator*() const -> Resource& {
	Resource* resource = resolve();
	assert(resource, std::source_location::current(), "Attempting to dereference an unloaded or invalid resource with ID: {}", owner_id());
	return *resource;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator==(const handle& other) const -> bool {
	if (!valid() || !other.valid()) {
		return false;
	}

	return owner_id() == other.owner_id() && m_loader == other.m_loader;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator!=(const handle& other) const -> bool {
	return !(*this == other);
}

template <typename Resource>
gse::resource::handle<Resource>::operator bool() const {
	return valid();
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::flush() -> void {
    std::vector<id> ids_to_load;

    {
        std::lock_guard lock(m_mutex);
        for (auto& slot : m_resources.items()) {
            if (slot.current_state.load(std::memory_order_acquire) == state::queued) {
                slot.current_state.store(state::loading, std::memory_order_release);

                const id rid = slot.resource.read()
                    ? slot.resource.read()->id()
                    : m_path_to_id[slot.path];

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
            {
                std::lock_guard lock(m_mutex);
                if (auto* slot = m_resources.try_get(rid)) {
                    if (!slot->resource.read()) {
                        slot->resource.write() = std::make_unique<R>(slot->path);
                        slot->resource.publish();
                    }
                    resource_ptr = slot->resource.read().get();
                } else {
                    update_state(rid, state::failed);
                    return;
                }
            }

            resource_ptr->load(m_context);
        });
    }

    m_load_group.post_range(jobs.begin(), jobs.end(), generate_id("resource.load"));
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::resource(id resource_id) -> void* {
	std::lock_guard lock(m_mutex);
	if (slot* slot_ptr = m_resources.try_get(resource_id)) {
		const auto s = slot_ptr->current_state.load(std::memory_order_acquire);
		if (s == state::loaded || s == state::reloading) {
			return const_cast<R*>(slot_ptr->resource.read().get());
		}
	}
	return nullptr;
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::resource_state(id resource_id) const -> state {
	std::lock_guard lock(m_mutex);
	if (const slot* slot_ptr = m_resources.try_get(resource_id); slot_ptr) {
		return slot_ptr->current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::resource_version(id resource_id) const -> std::uint32_t {
	std::lock_guard lock(m_mutex);
	if (const slot* slot_ptr = m_resources.try_get(resource_id); slot_ptr) {
		return slot_ptr->version.load(std::memory_order_acquire);
	}
	return 0;
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::update_state(id resource_id, state new_state) -> void {
	std::lock_guard lock(m_mutex);
	if (slot* slot_ptr = m_resources.try_get(resource_id)) {
		slot_ptr->current_state.store(new_state, std::memory_order_release);
	}
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::mark_for_gpu_finalization(id resource_id) -> void {
	m_context.mark_pending_for_finalization(typeid(Resource), resource_id);
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::finalize_state(id resource_id, size_t queue_size_before) -> void {
	if (m_context.gpu_queue_size() > queue_size_before) {
		m_context.mark_pending_for_finalization(typeid(Resource), resource_id);
	}
	else {
		update_state(resource_id, state::loaded);
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::queue_reload(id resource_id) -> void {
	std::lock_guard lock(m_reload_mutex);

	if (std::ranges::find(m_pending_reloads, resource_id) != m_pending_reloads.end()) {
		return;
	}

	m_pending_reloads.push_back(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::queue_reload_by_path(const std::filesystem::path& baked_path) -> void {
	std::lock_guard lock(m_mutex);

	auto it = m_path_to_id.find(baked_path);
	if (it == m_path_to_id.end()) {
		return;
	}

	queue_reload(it->second);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::queue_by_path(const std::filesystem::path& baked_path) -> void {
	std::lock_guard lock(m_mutex);

	if (m_path_to_id.contains(baked_path)) {
		return;
	}

	auto temp_resource = std::make_unique<R>(baked_path);
	const id resource_id = temp_resource->id();

	if (m_resources.add(resource_id, slot(std::move(temp_resource), state::queued, baked_path))) {
		m_path_to_id[baked_path] = resource_id;
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::finalize_reloads() -> void {
	std::vector<id> reloads_to_process;
	{
		std::lock_guard lock(m_reload_mutex);
		reloads_to_process.swap(m_pending_reloads);
	}

	for (const id rid : reloads_to_process) {
		slot* slot_ptr;
		{
			std::lock_guard lock(m_mutex);
			slot_ptr = m_resources.try_get(rid);
			if (!slot_ptr) {
				continue;
			}
		}

		const auto current_state = slot_ptr->current_state.load(std::memory_order_acquire);
		if (current_state != state::loaded && current_state != state::reloading) {
			continue;
		}

		slot_ptr->current_state.store(state::reloading, std::memory_order_release);

		auto new_resource = std::make_unique<R>(slot_ptr->path);
		new_resource->load(m_context);

		if (slot_ptr->resource.read()) {
			auto* old_resource = const_cast<R*>(slot_ptr->resource.read().get());
			old_resource->unload();
		}

		slot_ptr->resource.write() = std::move(new_resource);
		slot_ptr->resource.publish();
		slot_ptr->version.fetch_add(1, std::memory_order_release);
		slot_ptr->current_state.store(state::loaded, std::memory_order_release);

		std::println("[Hot Reload] Reloaded resource: {}", slot_ptr->path.filename().string());
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	return get_unlocked(id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	return get_unlocked(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::try_get(id id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	return try_get_unlocked(id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::try_get(const std::string& filename_no_ext) const -> handle<R> {
	if (!gse::exists(filename_no_ext)) {
		return handle<R>{};
	}
	const auto resource_id = gse::find(filename_no_ext);
	std::lock_guard lock(m_mutex);
	return try_get_unlocked(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::instantly_load(id resource_id) -> void {
	slot* slot_ptr;
	{
		std::lock_guard lock(m_mutex);
		slot_ptr = m_resources.try_get(resource_id);
		assert(slot_ptr, std::source_location::current(), "invalid id");
		if (slot_ptr->current_state == state::loaded)
			return;
	}

	while (slot_ptr->current_state.load(std::memory_order_acquire) == state::loading) {
		std::this_thread::yield();
	}

	if (slot_ptr->current_state.load(std::memory_order_acquire) == state::loaded) {
		return;
	}

	const gpu_work_token token(this, resource_id, m_context.gpu_queue_size());

	const bool work_was_queued = m_context.execute_and_detect_gpu_queue(
		[&](const auto& ctx) {
			if (!slot_ptr->resource.read()) {
				slot_ptr->resource.write() = std::make_unique<R>(slot_ptr->path);
				slot_ptr->resource.publish();
			}
			const_cast<R*>(slot_ptr->resource.read().get())->load(ctx);
		}
	);

	if (work_was_queued) {
		m_context.process_gpu_queue();
		m_context.config().device_config().device.waitIdle();
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
template <typename... Args> requires std::constructible_from<R, std::string, Args...>
auto gse::resource::loader<R, C>::queue(const std::string& name, Args&&... args) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			return get_unlocked(resource_id);
		}
	}

	auto temp_resource = std::make_unique<R>(name, std::forward<Args>(args)...);
	const auto resource_id = temp_resource->id();

	m_resources.add(resource_id, slot(std::move(temp_resource), state::queued, ""));
	return handle<R>(resource_id, this, 0);  // New resource starts at version 0
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::add(R&& resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto id = resource.id();
	assert(!m_resources.contains(id), std::source_location::current(), "Resource with ID {} already exists.", id);

	auto resource_ptr = std::make_unique<R>(std::move(resource));
	m_resources.add(id, slot(std::move(resource_ptr), state::loaded, ""));

	return handle<R>(id, this, 0);
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::get_unlocked(id id) const -> handle<Resource> {
	assert(m_resources.contains(id), std::source_location::current(), "Resource with ID {} not found in this loader.", id);
	return handle<Resource>(id, const_cast<loader*>(this), resource_version_unlocked(id));
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::try_get_unlocked(id id) const -> handle<Resource> {
	if (!m_resources.contains(id)) {
		return handle<Resource>{};
	}
	return handle<Resource>(id, const_cast<loader*>(this), resource_version_unlocked(id));
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::resource_version_unlocked(id resource_id) const -> std::uint32_t {
	if (const slot* slot_ptr = m_resources.try_get(resource_id); slot_ptr) {
		return slot_ptr->version.load(std::memory_order_acquire);
	}
	return 0;
}
