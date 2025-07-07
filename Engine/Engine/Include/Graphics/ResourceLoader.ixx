export module gse.graphics:resource_loader;

import std;

import gse.utility;
import gse.assert;

namespace gse {
	template<typename Resource, typename Context, typename... Args>
	concept resource =
		requires(Resource res, Args&&... a, Context& c) {
		Resource(std::forward<Args>(a)...);
		{ res.load(c) } -> std::same_as<void>;
		{ res.unload() } -> std::same_as<void>;
	}
	&& (
		requires(Resource res, Context& ctx) {
			{
				res.load(ctx)
			} -> std::same_as<void>;
		} || 
		requires(Resource res, Context & ctx) {
			{
				res.load(ctx.config())
			} -> std::same_as<void>;
		}
	);

	template <typename Handle, typename Resource>
	concept resource_handle = requires(const Resource& resource) {
		Handle(resource);
	};
}

export namespace gse {
	class resource_loader_base {
	public:
		enum struct state {
			unloaded,
			queued,
			loading,
			loaded,
			failed
		};

		virtual ~resource_loader_base() = default;
		virtual auto flush() -> void = 0;
		virtual auto get(const id& id) -> std::any = 0;
		virtual auto queue(
			const std::filesystem::path& path
		) -> id = 0;
		virtual auto resource_state(const id& id) -> state = 0;
	};

	template <typename Resource, typename Handle, typename RenderingContext>
		requires resource<Resource, RenderingContext> && resource_handle<Handle, Resource>
	class resource_loader : public resource_loader_base, public non_copyable {
	public:
		struct slot {
			std::unique_ptr<Resource> resource;
			std::atomic<state> current_state;
			std::filesystem::path path;

			slot(std::unique_ptr<Resource>&& res, const state s, const std::filesystem::path& p)
				: resource(std::move(res)), current_state(s), path(p) {}
		};

		explicit resource_loader(const RenderingContext& context) : m_context(context) {}
		~resource_loader() override;

		auto flush() -> void override;

		auto get(const id& id) -> std::any override;

		auto queue(
			const std::filesystem::path& path
		) -> id override;

		auto add(Resource&& resource) -> void;

		auto resource_state(const id& id) -> state override;
	private:
		const RenderingContext& m_context;
		std::unordered_map<id, slot> m_resources;
		std::vector<std::future<void>> m_loading_futures;
	};
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext> && gse::resource_handle<Handle, Resource>
gse::resource_loader<Resource, Handle, RenderingContext>::~resource_loader() {
	for (auto& future : m_loading_futures) {
		if (future.valid()) {
			future.wait();
		}
	}
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext>&& gse::resource_handle<Handle, Resource>
auto gse::resource_loader<Resource, Handle, RenderingContext>::flush() -> void {
	for (auto& slot : m_resources | std::views::values) {
		if (slot.current_state.load(std::memory_order_acquire) == state::queued) {
			slot.current_state.store(state::loading, std::memory_order_release);

			m_loading_futures.push_back(
				std::async(
					std::launch::async, 
					[this, &slot] {
						try {
							slot.resource = std::make_unique<Resource>(slot.path);
							if constexpr (
								requires {
									slot.resource->load(m_context);
								}
							) {
								slot.resource->load(m_context);
							}
							else {
								slot.resource->load(m_context.config());
							}
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

	auto ready = [](
		const std::future<void>& future
		) {
			return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		};

	m_loading_futures.erase(
		std::remove_if(m_loading_futures.begin(), m_loading_futures.end(), ready),
		m_loading_futures.end()
	);
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext>&& gse::resource_handle<Handle, Resource>
auto gse::resource_loader<Resource, Handle, RenderingContext>::get(const id& id) -> std::any {
	auto it = m_resources.find(id);
	if (it != m_resources.end()) {
		return Handle(*it->second.resource);
	}

	assert(false, std::format("Resource with ID {} not found.", id.number()));
	return {};
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext>&& gse::resource_handle<Handle, Resource>
auto gse::resource_loader<Resource, Handle, RenderingContext>::queue(const std::filesystem::path& path) -> id {
	const auto it = std::ranges::find_if(
		m_resources,
		[&](const auto& pair) {
			return pair.first.tag() == path.stem().string();
		}
	);
	
	if (it != m_resources.end()) {
		assert(false, std::format("Resource with name '{}' already exists.", path.stem().string()));
		return {};
	}

	auto temp_resource = std::make_unique<Resource>(path);
	const auto resource_id = temp_resource->id();

	m_resources.try_emplace(
		resource_id,
		std::move(temp_resource),
		state::queued,
		path
	);

	return resource_id;
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext>&& gse::resource_handle<Handle, Resource>
auto gse::resource_loader<Resource, Handle, RenderingContext>::add(Resource&& resource) -> void {
	const auto id = resource.id();

	assert(
		!m_resources.contains(id), 
		std::format("Resource with ID {} already exists.", id.number())
	);

	m_resources.try_emplace(
		id,
		std::make_unique<Resource>(std::move(resource)),
		state::loaded,
		resource.path()
	);
}

template <typename Resource, typename Handle, typename RenderingContext>
	requires gse::resource<Resource, RenderingContext>&& gse::resource_handle<Handle, Resource>
auto gse::resource_loader<Resource, Handle, RenderingContext>::resource_state(const id& id) -> state {
	if (auto it = m_resources.find(id); it != m_resources.end()) {
		return it->second.current_state.load(std::memory_order_acquire);
	}
	return state::unloaded;
}
