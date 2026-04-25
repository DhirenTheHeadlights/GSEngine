export module gse.ecs:update_context;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :access_token;
import :component;
import :phase_context;
import :registry;
import :task_context;
import :traits;

export namespace gse {
	class update_context : public task_context {
	public:
		update_context(
			void* gpu_ctx,
			const state_snapshot_provider& snapshots,
			channel_writer& channels,
			const channel_reader_provider& channel_reader,
			const resources_provider& resources,
			task_graph& graph,
			registry& reg,
			async::rw_mutex_registry& access_mutexes
		);

		template <typename... Accesses>
		auto acquire(
		) -> async::task<std::tuple<Accesses...>>;

		template <is_component T>
		auto try_component(
			id owner
		) const -> const T*;

		template <is_component T>
		auto components(
		) const -> std::span<const T>;

		template <is_component T>
		auto drain_component_adds(
		) -> std::vector<id>;

		template <is_component T>
		auto drain_component_updates(
		) -> std::vector<id>;

		template <is_component T>
		auto drain_component_removes(
		) -> std::vector<id>;

		auto ensure_exists(
			id owner
		) -> void;

		auto active(
			id owner
		) const -> bool;

		auto ensure_active(
			id owner
		) -> void;

		auto add_deferred_action(
			id owner,
			registry::deferred_action action
		) -> void;

		template <is_component T, typename... Args>
		auto defer_add(
			id entity,
			Args&&... args
		) -> void;

		template <is_component T>
		auto defer_remove(
			id entity
		) -> void;

		auto defer_activate(
			id entity
		) -> void;

	private:
		registry& m_reg;
		async::rw_mutex_registry& m_access_mutexes;
	};
}

namespace gse {
	template <typename Access>
	auto acquire_lock_for(
		async::rw_mutex_registry& registry
	) -> async::task<>;

	template <typename Access>
	auto make_locked_handle(
		registry& reg,
		async::rw_mutex_registry& mutex_registry
	) -> Access;

	template <typename Access>
	auto access_trace_label(
	) -> std::string;

	template <typename... Accesses>
	auto acquire_trace_id(
	) -> id;
}

gse::update_context::update_context(
	void* gpu_ctx,
	const state_snapshot_provider& snapshots,
	channel_writer& channels,
	const channel_reader_provider& channel_reader,
	const resources_provider& resources,
	task_graph& graph,
	registry& reg,
	async::rw_mutex_registry& access_mutexes
) : task_context{ gpu_ctx, snapshots, channels, channel_reader, resources, graph },
	m_reg(reg),
	m_access_mutexes(access_mutexes) {}

template <typename Access>
auto gse::acquire_lock_for(async::rw_mutex_registry& registry) -> async::task<> {
	using element_t = access_element_t<Access>;
	auto& mutex = registry.mutex_for(id_of<element_t>());
	if constexpr (is_read_access_v<Access>) {
		co_await mutex.lock_shared();
	}
	else {
		co_await mutex.lock_exclusive();
	}
}

template <typename Access>
auto gse::make_locked_handle(registry& reg, async::rw_mutex_registry& mutex_registry) -> Access {
	using element_t = access_element_t<Access>;
	auto& mutex = mutex_registry.mutex_for(id_of<element_t>());
	if constexpr (is_read_access_v<Access>) {
		return reg.template acquire_read<element_t>(&mutex);
	}
	else {
		return reg.template acquire_write<element_t>(&mutex);
	}
}

template <typename Access>
auto gse::access_trace_label() -> std::string {
	const std::string_view tag = is_read_access_v<Access> ? "read" : "write";
	return std::format("{}<{}>", tag, type_tag<access_element_t<Access>>());
}

template <typename... Accesses>
auto gse::acquire_trace_id() -> id {
	static const id cached = []{
		std::string label = "acquire<";
		bool first = true;
		const auto append_label = [&](const std::string& part) {
			if (!first) {
				label += ", ";
			}
			label += part;
			first = false;
		};
		(append_label(access_trace_label<Accesses>()), ...);
		label += ">";
		return find_or_generate_id(label);
	}();
	return cached;
}

template <typename... Accesses>
auto gse::update_context::acquire() -> async::task<std::tuple<Accesses...>> {
	constexpr std::size_t count = sizeof...(Accesses);

	constexpr std::array<id, count> type_ids = {
		id_of<access_element_t<Accesses>>()...
	};

	std::array<std::size_t, count> order;
	std::iota(order.begin(), order.end(), std::size_t{ 0 });
	std::ranges::sort(order, [&](const std::size_t a, const std::size_t b) {
		return type_ids[a] < type_ids[b];
	});

	using lock_fn = async::task<>(*)(async::rw_mutex_registry&);
	constexpr std::array<lock_fn, count> fns = { &acquire_lock_for<Accesses>... };

	static const id tid = acquire_trace_id<Accesses...>();
	const auto key = trace::allocate_async_key();
	trace::begin_async(tid, key);

	for (std::size_t i = 0; i < count; ++i) {
		co_await fns[order[i]](m_access_mutexes);
	}

	trace::end_async(tid, key);

	co_return std::tuple<Accesses...>{ make_locked_handle<Accesses>(m_reg, m_access_mutexes)... };
}

template <gse::is_component T>
auto gse::update_context::try_component(const id owner) const -> const T* {
	return m_reg.try_component<T>(owner);
}

template <gse::is_component T>
auto gse::update_context::components() const -> std::span<const T> {
	return m_reg.components<T>();
}

template <gse::is_component T>
auto gse::update_context::drain_component_adds() -> std::vector<id> {
	return m_reg.drain_component_adds<T>();
}

template <gse::is_component T>
auto gse::update_context::drain_component_updates() -> std::vector<id> {
	return m_reg.drain_component_updates<T>();
}

template <gse::is_component T>
auto gse::update_context::drain_component_removes() -> std::vector<id> {
	return m_reg.drain_component_removes<T>();
}

auto gse::update_context::ensure_exists(const id owner) -> void {
	m_reg.ensure_exists(owner);
}

auto gse::update_context::active(const id owner) const -> bool {
	return m_reg.active(owner);
}

auto gse::update_context::ensure_active(const id owner) -> void {
	m_reg.ensure_active(owner);
}

auto gse::update_context::add_deferred_action(const id owner, registry::deferred_action action) -> void {
	m_reg.add_deferred_action(owner, std::move(action));
}

template <gse::is_component T, typename... Args>
auto gse::update_context::defer_add(const id entity, Args&&... args) -> void {
	m_reg.defer_add_component<T>(entity, std::forward<Args>(args)...);
}

template <gse::is_component T>
auto gse::update_context::defer_remove(const id entity) -> void {
	m_reg.defer_remove_component<T>(entity);
}

auto gse::update_context::defer_activate(const id entity) -> void {
	m_reg.defer_activate(entity);
}
