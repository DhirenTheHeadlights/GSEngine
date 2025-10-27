export module gse.graphics:resource_loader;

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
		failed
	};

	template <typename Resource>
	class handle {
	public:
		handle() = default;
		handle(
			const id& resource_id,
			loader_base* loader
		);

		[[nodiscard]] auto resolve(
		) const -> Resource*;

		[[nodiscard]] auto state(
		) const -> state;

		[[nodiscard]] auto valid(
		) const -> bool;

		[[nodiscard]] auto id(
		) const -> const id& ;

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
		gse::id m_id;
		loader_base* m_loader = nullptr;
	};
}

export namespace gse::resource {
	class loader_base {
	public:
		virtual ~loader_base() = default;
		virtual auto flush() -> void = 0;
		virtual auto compile() -> void = 0;
		[[nodiscard]] virtual auto resource(const id& resource_id) -> void* = 0;
		[[nodiscard]] virtual auto resource_state(const id& resource_id) const -> state = 0;
		virtual auto update_state(const id& resource_id, state new_state) -> void = 0;
		virtual auto mark_for_gpu_finalization(const id& resource_id) -> void = 0;
		virtual auto finalize_state(const id& resource_id, size_t queue_size_before) -> void = 0;
	};

	class gpu_work_token final : public non_copyable {
	public:
		gpu_work_token(loader_base* loader, const id& resource_id, const std::size_t queue_size_before)
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

	template <typename Resource, typename RenderingContext>
		requires gse::is_resource<Resource, RenderingContext>
	class loader final : public loader_base, public non_copyable {
	public:
		struct slot {
			std::unique_ptr<Resource> resource;
			std::atomic<state> current_state;
			std::filesystem::path path;

			slot(std::unique_ptr<Resource>&& res, const state s, const std::filesystem::path& p)
				: resource(std::move(res)), current_state(s), path(p) {
			}

			slot(slot&& other) noexcept
				: resource(std::move(other.resource)),
				current_state(other.current_state.load(std::memory_order_relaxed)),
				path(std::move(other.path)) {
			}

			auto operator=(slot&& other) noexcept -> slot& {
				if (this != &other) {
					resource = std::move(other.resource);
					current_state.store(other.current_state.load(std::memory_order_relaxed));
					path = std::move(other.path);
				}
				return *this;
			}

			slot(const slot&) = delete;
			auto operator=(const slot&) -> slot & = delete;
		};

		explicit loader(RenderingContext& context) : m_context(context) {}
		~loader() override = default;

		auto flush() -> void override;
		auto compile() -> void override;

		[[nodiscard]] auto resource(const id& resource_id) -> void* override;
		[[nodiscard]] auto resource_state(const id& resource_id) const -> state override;

		auto update_state(const id& resource_id, state new_state) -> void override;
		auto mark_for_gpu_finalization(const id& resource_id) -> void override;
		auto finalize_state(const id& resource_id, size_t queue_size_before) -> void override;

		auto get(const id& id) const -> handle<Resource>;
		auto get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto instantly_load(const id& resource_id) -> void;

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

		auto get_unlocked(const id& id) const -> handle<Resource> ;
	};
}

template <typename Resource>
gse::resource::handle<Resource>::handle(const gse::id& resource_id, loader_base* loader): m_id(resource_id), m_loader(loader) {}

template <typename Resource>
auto gse::resource::handle<Resource>::resolve() const -> Resource* {
	if (!m_loader) return nullptr;
	return static_cast<Resource*>(m_loader->resource(m_id));
}

template <typename Resource>
auto gse::resource::handle<Resource>::state() const -> resource::state {
	if (!m_loader) return state::unloaded;
	return m_loader->resource_state(m_id);
}

template <typename Resource>
auto gse::resource::handle<Resource>::valid() const -> bool {
	return m_loader && state() == state::loaded;
}

template <typename Resource>
auto gse::resource::handle<Resource>::id() const -> const gse::id& {
	return m_id;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator->() const -> Resource* {
	Resource* resource = resolve();
	assert(resource, std::source_location::current(), "Attempting to access an unloaded or invalid resource with ID: {}", m_id);
	return resource;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator*() const -> Resource& {
	Resource* resource = resolve();
	assert(resource, std::source_location::current(), "Attempting to dereference an unloaded or invalid resource with ID: {}", m_id);
	return *resource;
}

template <typename Resource>
auto gse::resource::handle<Resource>::operator==(const handle& other) const -> bool {
	return m_id == other.m_id && m_loader == other.m_loader;
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

                const id rid = slot.resource
                    ? slot.resource->id()
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

            R* resource_ptr = nullptr;
            {
                std::lock_guard lock(m_mutex);
                if (auto* slot = m_resources.try_get(rid)) {
                    if (!slot->resource) {
                        slot->resource = std::make_unique<R>(slot->path);
                    }
                    resource_ptr = slot->resource.get();
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
auto gse::resource::loader<R, C>::compile() -> void {
    const auto paths = R::compile();

    std::lock_guard lock(m_mutex);
    for (const auto& path : paths) {
        auto temp_resource = std::make_unique<R>(path);
        const auto resource_id = temp_resource->id();
        if (m_resources.add(resource_id, slot(std::move(temp_resource), state::queued, path))) {
            m_path_to_id[path] = resource_id;
        }
    }
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::resource(const id& resource_id) -> void* {
	std::lock_guard lock(m_mutex);
	slot* slot_ptr = m_resources.try_get(resource_id);
	if (slot_ptr && slot_ptr->current_state.load(std::memory_order_acquire) == state::loaded) {
		return slot_ptr->resource.get();
	}
	return nullptr;
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::resource_state(const id& resource_id) const -> state {
	std::lock_guard lock(m_mutex);
	if (const slot* slot_ptr = m_resources.try_get(resource_id); slot_ptr) {
		return slot_ptr->current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::update_state(const id& resource_id, state new_state) -> void {
	std::lock_guard lock(m_mutex);
	if (slot* slot_ptr = m_resources.try_get(resource_id)) {
		slot_ptr->current_state.store(new_state, std::memory_order_release);
	}
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::mark_for_gpu_finalization(const id& resource_id) -> void {
	m_context.mark_pending_for_finalization(typeid(Resource), resource_id);
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::finalize_state(const id& resource_id, size_t queue_size_before) -> void {
	if (m_context.gpu_queue_size() > queue_size_before) {
		m_context.mark_pending_for_finalization(typeid(Resource), resource_id);
	}
	else {
		update_state(resource_id, state::loaded);
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(const id& id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	return get_unlocked(id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(const std::string& filename_no_ext) const -> handle<R> {
	const auto resource_id = gse::find(filename_no_ext);
	return get_unlocked(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::instantly_load(const id& resource_id) -> void {
	slot* slot_ptr;
	{
		std::lock_guard lock(m_mutex);
		slot_ptr = m_resources.try_get(resource_id);
		assert(slot_ptr, std::source_location::current(), "invalid id");
		if (slot_ptr->current_state == state::loaded ||
			slot_ptr->current_state == state::loading)
			return;
	}

	const gpu_work_token token(this, resource_id, m_context.gpu_queue_size());

	const bool work_was_queued = m_context.execute_and_detect_gpu_queue(
		[&](const auto& ctx) {
			if (!slot_ptr->resource) {
				slot_ptr->resource = std::make_unique<R>(slot_ptr->path);
			}
			slot_ptr->resource->load(ctx);
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
	return handle<R>(resource_id, this);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::add(R&& resource) -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto id = resource.id();
	assert(!m_resources.contains(id), std::source_location::current(), "Resource with ID {} already exists.", id);

	auto resource_ptr = std::make_unique<R>(std::move(resource));
	m_resources.add(id, slot(std::move(resource_ptr), state::loaded, ""));

	return handle<R>(id, this);
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::get_unlocked(const id& id) const -> handle<Resource> {
	assert(m_resources.contains(id), std::source_location::current(), "Resource with ID {} not found in this loader.", id);
	return handle<Resource>(id, const_cast<loader*>(this));
}
