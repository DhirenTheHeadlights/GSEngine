export module gse.ecs:update_context;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :access_token;
import :component;
import :phase_context;
import :registries;
import :registry;
import :task_context;
import :traits;

export namespace gse {
	class update_context : public task_context {
	public:
		update_context(
			void* gpu_ctx,
			state_registry& states,
			resource_registry& resources_store,
			channel_registry& channels_store,
			channel_writer& channels,
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

		auto exists(
			id owner
		) const -> bool;

		auto active(
			id owner
		) const -> bool;

		auto ensure_active(
			id owner
		) -> void;

		auto remove(
			id owner
		) -> void;

		template <is_component T> requires has_params<T>
		auto add_component(
			id owner,
			const typename T::params& p
		) -> T*;

		template <is_component T> requires (!std::same_as<typename T::params, typename T::network_data_t>)
		auto add_component(
			id owner,
			const typename T::network_data_t& nd
		) -> T*;

		template <is_component T>
		auto add_component(
			id owner
		) -> T*;

		template <is_component T>
		auto remove_component(
			id owner
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
	state_registry& states,
	resource_registry& resources_store,
	channel_registry& channels_store,
	channel_writer& channels,
	task_graph& graph,
	registry& reg,
	async::rw_mutex_registry& access_mutexes
) : task_context{ gpu_ctx, states, resources_store, channels_store, channels, graph, true },
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

auto gse::update_context::exists(const id owner) const -> bool {
	return m_reg.exists(owner);
}

auto gse::update_context::active(const id owner) const -> bool {
	return m_reg.active(owner);
}

auto gse::update_context::ensure_active(const id owner) -> void {
	m_reg.ensure_active(owner);
}

auto gse::update_context::remove(const id owner) -> void {
	m_reg.remove(owner);
}

template <gse::is_component T> requires gse::has_params<T>
auto gse::update_context::add_component(const id owner, const typename T::params& p) -> T* {
	return m_reg.add_component<T>(owner, p);
}

template <gse::is_component T> requires (!std::same_as<typename T::params, typename T::network_data_t>)
auto gse::update_context::add_component(const id owner, const typename T::network_data_t& nd) -> T* {
	return m_reg.add_component<T>(owner, nd);
}

template <gse::is_component T>
auto gse::update_context::add_component(const id owner) -> T* {
	return m_reg.add_component<T>(owner);
}

template <gse::is_component T>
auto gse::update_context::remove_component(const id owner) -> void {
	m_reg.remove_component<T>(owner);
}
