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
		handle(const id& resource_id, loader_base* loader) : m_id(resource_id), m_loader(loader) {}

		[[nodiscard]] auto resolve() const -> Resource*;
		[[nodiscard]] auto state() const -> state;
		[[nodiscard]] auto valid() const -> bool;
		[[nodiscard]] auto id() const -> const id& { return m_id; }

		[[nodiscard]] auto operator->() const -> Resource* {
			Resource* resource = resolve();
			assert(resource, std::format("Attempting to access an unloaded or invalid resource with ID: {}", m_id));
			return resource;
		}

		[[nodiscard]] auto operator*() const -> Resource& {
			Resource* resource = resolve();
			assert(resource, std::format("Attempting to dereference an unloaded or invalid resource with ID: {}", m_id));
			return *resource;
		}

		explicit operator bool() const { return valid(); }
	private:
		gse::id m_id;
		loader_base* m_loader = nullptr;
	};

	class loader_base {
	public:
		virtual ~loader_base() = default;
		virtual auto flush() -> void = 0;
		virtual auto compile() -> void = 0;
		[[nodiscard]] virtual auto resource(const id& resource_id) -> void* = 0;
		[[nodiscard]] virtual auto resource_state(const id& resource_id) const -> state = 0;
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

		explicit loader(const RenderingContext& context) : m_context(context) {}
		~loader() override;

		auto flush() -> void override;
		auto compile() -> void override;

		[[nodiscard]] auto resource(const id& resource_id) -> void* override;
		[[nodiscard]] auto resource_state(const id& resource_id) const -> state override;

		auto get(const id& id) const -> handle<Resource>;
		auto get(const std::string& filename_no_ext) const -> handle<Resource>;
		auto instantly_load(const id& resource_id) -> void;

		template <typename... Args>
			requires std::constructible_from<Resource, std::string, Args...>
		auto queue(const std::string& name, Args&&... args) -> handle<Resource>;

		auto add(Resource&& resource) -> handle<Resource>;
	private:
		const RenderingContext& m_context;
		id_mapped_collection<slot, id> m_resources;
		std::unordered_map<std::filesystem::path, id> m_path_to_id;
		std::vector<std::future<void>> m_loading_futures;
		mutable std::recursive_mutex m_mutex;
	};
}

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

template <typename R, typename C> requires gse::is_resource<R, C>
gse::resource::loader<R, C>::~loader() {
	for (auto& future : m_loading_futures) {
		if (future.valid()) future.wait();
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::flush() -> void {
	std::vector<slot*> slots_to_load;

	{
		std::lock_guard lock(m_mutex);
		for (auto& slot : m_resources.items()) {
			if (slot.current_state.load(std::memory_order_acquire) == state::queued) {
				slot.current_state.store(state::loading, std::memory_order_release);
				slots_to_load.push_back(&slot);
			}
		}
	} 

	for (auto* slot_ptr : slots_to_load) {
		m_loading_futures.push_back(
			std::async(
				std::launch::async,
				[this, slot_ptr] {
					try {
						if (!slot_ptr->resource) {
							slot_ptr->resource = std::make_unique<R>(slot_ptr->path);
						}
						slot_ptr->resource->load(m_context);
						slot_ptr->current_state.store(state::loaded, std::memory_order_release);
					}
					catch (...) {
						slot_ptr->current_state.store(state::failed, std::memory_order_release);
					}
				}
			)
		);
	}

	std::erase_if(m_loading_futures, [](const auto& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::compile() -> void {
	std::lock_guard lock(m_mutex);
	for (const auto& path : R::compile()) {
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

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(const id& id) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	assert(m_resources.contains(id), std::format("Resource with ID {} not found in this loader.", id));
	return handle<R>(id, const_cast<loader*>(this));
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::get(const std::string& filename_no_ext) const -> handle<R> {
	std::lock_guard lock(m_mutex);
	const auto resource_id = gse::find(filename_no_ext);
	assert(m_resources.contains(resource_id), std::format("Resource with tag '{}' (ID {}) not found in this loader.", filename_no_ext, resource_id));
	return get(resource_id);
}

template <typename R, typename C> requires gse::is_resource<R, C>
auto gse::resource::loader<R, C>::instantly_load(const id& resource_id) -> void {
	std::lock_guard lock(m_mutex);
	slot* slot_ptr = m_resources.try_get(resource_id);
	assert(slot_ptr, std::format("Cannot instantly load resource with ID {}, not found.", resource_id));

	auto current_state = slot_ptr->current_state.load(std::memory_order_acquire);
	if (current_state == state::loaded || current_state == state::loading) return;

	try {
		if (!slot_ptr->resource) {
			slot_ptr->resource = std::make_unique<R>(slot_ptr->path);
		}
		slot_ptr->resource->load(m_context);
		slot_ptr->current_state.store(state::loaded, std::memory_order_release);
	}
	catch (...) {
		slot_ptr->current_state.store(state::failed, std::memory_order_release);
	}
}

template <typename R, typename C> requires gse::is_resource<R, C>
template <typename... Args> requires std::constructible_from<R, std::string, Args...>
auto gse::resource::loader<R, C>::queue(const std::string& name, Args&&... args) -> handle<R> {
	std::lock_guard lock(m_mutex);
	if (exists(name)) {
		if (const auto resource_id = gse::find(name); m_resources.contains(resource_id)) {
			return get(resource_id);
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
	assert(!m_resources.contains(id), std::format("Resource with ID {} already exists.", id));

	auto resource_ptr = std::make_unique<R>(std::move(resource));
	m_resources.add(id, slot(std::move(resource_ptr), state::loaded, ""));

	return handle<R>(id, this);
}
