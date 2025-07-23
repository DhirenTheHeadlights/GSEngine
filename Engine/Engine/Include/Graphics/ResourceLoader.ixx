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


		explicit operator bool() const {
			return valid();
		}

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

			slot(std::unique_ptr<Resource>&& res, const state s, const std::filesystem::path& p) : resource(std::move(res)), current_state(s), path(p) {}
		};

		explicit loader(RenderingContext& context) : m_context(context) {}
		~loader() override;

		auto flush() -> void override;
		auto compile() -> void override;

		[[nodiscard]] auto resource(const id& resource_id) -> void* override;
		[[nodiscard]] auto resource_state(const id& resource_id) const -> state override;

		auto get(const id& id) -> handle<Resource>;
		auto get(const std::string& filename) -> handle<Resource>;
		auto instantly_load(const id& resource_id) -> void;

		template <typename... Args>
			requires std::constructible_from<Resource, std::string, Args...>
		auto queue(const std::string& name, Args&&... args) -> handle<Resource>;

		auto add(Resource&& resource) -> handle<Resource>;

	private:
		RenderingContext& m_context;
		std::unordered_map<id, slot> m_resources;
		std::vector<std::future<void>> m_loading_futures;
	};
}

template <typename Resource>
auto gse::resource::handle<Resource>::resolve() const -> Resource* {
	return static_cast<Resource*>(m_loader->resource(m_id));
}

template <typename Resource>
auto gse::resource::handle<Resource>::state() const -> resource::state {
	if (!m_loader) {
		return state::unloaded;
	}
	return m_loader->resource_state(m_id);
}

template <typename Resource>
auto gse::resource::handle<Resource>::valid() const -> bool {
	return m_loader && state() == state::loaded;
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
gse::resource::loader<Resource, RenderingContext>::~loader() {
	for (auto& future : m_loading_futures) {
		if (future.valid()) {
			future.wait();
		}
	}
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::flush() -> void {
	for (auto& pair : m_resources) {
		if (auto& slot = pair.second; slot.current_state.load(std::memory_order_acquire) == state::queued) {
			slot.current_state.store(state::loading, std::memory_order_release);

			m_loading_futures.push_back(
				std::async(
					std::launch::async,
					[this, &slot] {
						try {
							if (!slot.resource) {
								slot.resource = std::make_unique<Resource>(slot.path);
							}
							slot.resource->load(m_context);
							slot.current_state.store(state::loaded, std::memory_order_release);
						}
						catch (const std::exception& e) {
							std::println("Failed to load resource '{}': {}", slot.path.string(), e.what());
							slot.current_state.store(state::failed, std::memory_order_release);
						}
						catch (...) {
							slot.current_state.store(state::failed, std::memory_order_release);
						}
					}
				)
			);
		}
	}

	std::erase_if(
		m_loading_futures, 
		[](const auto& future) {
			return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}
	);
}

template <typename Resource, typename RenderingContext> requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::compile() -> void {
	for (const auto& path : Resource::compile()) {
		auto temp_resource = std::make_unique<Resource>(path);
		const auto resource_id = temp_resource->id();

		m_resources.try_emplace(
			resource_id,
			std::move(temp_resource),
			state::queued,
			path
		);
	}
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::resource(const id& resource_id) -> void* {
	auto it = m_resources.find(resource_id);
	if (it != m_resources.end() && it->second.current_state.load(std::memory_order_acquire) == state::loaded) {
		return it->second.resource.get();
	}
	return nullptr;
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::resource_state(const id& resource_id) const -> state {
	if (auto it = m_resources.find(resource_id); it != m_resources.end()) {
		return it->second.current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::get(const id& id) -> handle<Resource> {
	const auto it = m_resources.find(id);

	assert(it != m_resources.end(), std::format("Resource with ID {} not found.", id));

	return handle<Resource>(id, this);
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::get(const std::string& filename) -> handle<Resource> {
	const auto id = find(filename);
	const auto it = m_resources.find(id);

	assert(it != m_resources.end(), std::format("Resource with ID {} not found.", id));

	return handle<Resource>(it->first, this);
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::instantly_load(const id& resource_id) -> void {
	auto it = m_resources.find(resource_id);
	if (it == m_resources.end()) {
		assert(false, std::format("Cannot instantly load resource with ID {}, not found.", resource_id.number()));
		return;
	}

	auto& slot = it->second;
	auto current_state = slot.current_state.load(std::memory_order_acquire);

	if (current_state == state::loaded || current_state == state::loading) {
		return;
	}

	try {
		if (!slot.resource) {
			slot.resource = std::make_unique<Resource>(slot.path);
		}
		slot.resource->load(m_context);
		slot.current_state.store(state::loaded, std::memory_order_release);
	}
	catch (const std::exception& e) {
		std::println("Failed to instantly load resource '{}': {}", slot.path.string(), e.what());
		slot.current_state.store(state::failed, std::memory_order_release);
	}
	catch (...) {
		slot.current_state.store(state::failed, std::memory_order_release);
	}
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
template <typename... Args>
	requires std::constructible_from<Resource, std::string, Args...>
auto gse::resource::loader<Resource, RenderingContext>::queue(const std::string& name, Args&&... args) -> handle<Resource> {
	const auto it = std::ranges::find_if(
		m_resources,
		[&](const auto& pair) {
			return pair.first.tag() == name;
		}
	);

	if (it != m_resources.end()) {
		return handle<Resource>(it->first, this);
	}

	auto temp_resource = std::make_unique<Resource>(name, std::forward<Args>(args)...);
	const auto resource_id = temp_resource->id();

	m_resources.try_emplace(
		resource_id,
		std::move(temp_resource),
		state::loaded,
		std::filesystem::path()
	);

	return handle<Resource>(resource_id, this);
}

template <typename Resource, typename RenderingContext>
	requires gse::is_resource<Resource, RenderingContext>
auto gse::resource::loader<Resource, RenderingContext>::add(Resource&& resource) -> handle<Resource> {
	const auto id = resource.id();

	assert(
		!m_resources.contains(id),
		std::format("Resource with ID {} already exists.", id.number())
	);

	auto resource_ptr = std::make_unique<Resource>(std::move(resource));

	m_resources.try_emplace(
		id,
		std::move(resource_ptr),
		state::loaded,
		std::filesystem::path()
	);

	return handle<Resource>(id, this);
}