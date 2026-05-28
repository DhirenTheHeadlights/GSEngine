export module gse.ecs:run_context;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;

import :access_token;
import :component;
import :registries;
import :registry;
import :task_context;
import :traits;

export namespace gse {
	class scheduler;

	template <typename T>
	struct read_tag {};

	template <typename T>
	struct write_tag {};

	template <typename T>
	constexpr read_tag<T> read_v{};

	template <typename T>
	constexpr write_tag<T> write_v{};

	template <typename Tag>
	struct tag_to_access;

	template <typename T>
	struct tag_to_access<read_tag<T>> {
		using type = read<T>;
	};

	template <typename T>
	struct tag_to_access<write_tag<T>> {
		using type = write<T>;
	};

	template <typename Tag>
	using tag_to_access_t = typename tag_to_access<Tag>::type;

	class run_context : public task_context {
	public:
		run_context(
			scheduler& sched,
			state_registry& states,
			resource_registry& resources_store,
			channel_registry& channels_store,
			channel_writer& channels,
			task_graph& graph,
			registry& reg,
			async::rw_mutex_registry& access_mutexes,
			async::manual_event& resume_event,
			async::manual_event& paused_event,
			bool& is_in_update_loop,
			bool& settled
		);

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> void;

		[[nodiscard]] auto next_tick() -> async::task<>;

		[[nodiscard]] auto yield_tick() -> async::task<>;

		template <typename... Accesses>
		auto acquire() -> async::task<std::tuple<Accesses...>>;

		template <typename... Tags>
		auto acquire_with(
			Tags... tags
		) -> async::task<std::tuple<tag_to_access_t<Tags>...>>;

		template <typename T>
		auto try_component(
			id owner
		) const -> const T*;

		template <typename T>
		auto components() const -> std::span<const T>;

		template <typename T>
		auto drain_component_adds() -> std::vector<id>;

		template <typename T>
		auto drain_component_updates() -> std::vector<id>;

		template <typename T>
		auto drain_component_removes() -> std::vector<id>;

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

		template <typename T>
		auto add_component(
			id owner,
			T value = T{}
		) -> T*;

		template <typename T>
		auto remove_component(
			id owner
		) -> void;

		template <typename T>
		auto ensure_storage() -> void;

		[[nodiscard]] auto registry() const -> gse::registry&;

		[[nodiscard]] auto held_lock_count() const -> int;

	private:
		scheduler& m_sched;
		gse::registry& m_reg;
		async::rw_mutex_registry& m_access_mutexes;
		std::atomic<int> m_held_locks{ 0 };
		async::manual_event& m_resume_event;
		async::manual_event& m_paused_event;
		bool& m_is_in_update_loop;
		bool& m_settled;
	};
}

namespace gse {
	using lock_fn = async::task<> (
			*
	)(
		async::rw_mutex_registry&,
		id
	);

	auto acquire_shared(
		async::rw_mutex_registry& mutexes,
		id type
	) -> async::task<>;

	auto acquire_exclusive(
		async::rw_mutex_registry& mutexes,
		id type
	) -> async::task<>;

	auto acquire_locks_in_sorted_order(
		async::rw_mutex_registry& mutexes,
		std::span<const id> type_ids,
		std::span<const lock_fn> fns,
		id trace_id
	) -> async::task<>;

	auto make_acquire_trace_id(
		std::span<const std::string> labels
	) -> id;

	auto format_access_label(
		std::string_view tag,
		std::string_view type_name
	) -> std::string;

	template <typename Access>
	auto make_locked_handle(
		access_token token,
		registry& reg,
		async::rw_mutex_registry& mutex_registry,
		std::atomic<int>* held_locks
	) -> Access;

	template <typename Access>
	auto access_trace_label() -> std::string;

	template <typename... Accesses>
	auto acquire_trace_id() -> id;
}

gse::run_context::run_context(scheduler& sched, state_registry& states, resource_registry& resources_store, channel_registry& channels_store, channel_writer& channels, task_graph& graph, gse::registry& reg, async::rw_mutex_registry& access_mutexes, async::manual_event& resume_event, async::manual_event& paused_event, bool& is_in_update_loop, bool& settled)
	: task_context{ states, resources_store, channels_store, channels, graph, true }, m_sched(sched), m_reg(reg), m_access_mutexes(access_mutexes), m_resume_event(resume_event), m_paused_event(paused_event), m_is_in_update_loop(is_in_update_loop), m_settled(settled) {
}

auto gse::run_context::next_tick() -> async::task<> {
	const int locks = held_lock_count();
	assert(
		locks == 0,
		"system held {} component lock(s) across co_await ctx.next_tick(); scope your acquire<> so the locked handle "
		"is destroyed before next_tick",
		locks
	);
	m_is_in_update_loop = true;
	m_settled = true;
	m_paused_event.set();
	co_await m_resume_event.wait();
	m_resume_event.reset();
}

auto gse::run_context::yield_tick() -> async::task<> {
	const int locks = held_lock_count();
	assert(
		locks == 0,
		"system held {} component lock(s) across co_await ctx.yield_tick(); scope your acquire<> so the locked handle "
		"is destroyed before yield_tick",
		locks
	);
	m_paused_event.set();
	co_await m_resume_event.wait();
	m_resume_event.reset();
}

auto gse::acquire_shared(async::rw_mutex_registry& mutexes, const id type) -> async::task<> {
	auto& mutex = mutexes.mutex_for(type);
	co_await mutex.lock_shared();
}

auto gse::acquire_exclusive(async::rw_mutex_registry& mutexes, const id type) -> async::task<> {
	auto& mutex = mutexes.mutex_for(type);
	co_await mutex.lock_exclusive();
}

auto gse::acquire_locks_in_sorted_order(async::rw_mutex_registry& mutexes, const std::span<const id> type_ids, const std::span<const lock_fn> fns, const id trace_id) -> async::task<> {
	constexpr std::size_t max_arity = 16;
	const std::size_t count = type_ids.size();
	assert(count <= max_arity, "acquire arity {} exceeds max {}", count, max_arity);

	std::array<std::size_t, max_arity> order_buf{};
	const std::span order(order_buf.data(), count);
	std::ranges::iota(
		order,
		std::size_t{ 0 }
	);
	std::ranges::sort(
		order,
		[type_ids](const std::size_t a, const std::size_t b) {
			return type_ids[a] < type_ids[b];
		}
	);

	const auto key = trace::allocate_async_key();
	trace::begin_async(trace_id, key);
	for (const std::size_t i : order) {
		co_await fns[i](mutexes, type_ids[i]);
	}
	trace::end_async(trace_id, key);
}

auto gse::format_access_label(const std::string_view tag, const std::string_view type_name) -> std::string {
	return std::format("{}<{}>", tag, type_name);
}

auto gse::make_acquire_trace_id(const std::span<const std::string> labels) -> id {
	std::string out = "acquire<";
	bool first = true;
	for (const auto& part : labels) {
		if (!first) {
			out += ", ";
		}
		out += part;
		first = false;
	}
	out += ">";
	return find_or_generate_id(out);
}

template <typename Access>
auto gse::make_locked_handle(access_token token, registry& reg, async::rw_mutex_registry& mutex_registry, std::atomic<int>* held_locks) -> Access {
	using element_t = access_element_t<Access>;
	auto& mutex = mutex_registry.mutex_for(id_of<element_t>());
	if constexpr (is_read_access_v<Access>) {
		return reg.template acquire_read<element_t>(std::move(token), &mutex, held_locks);
	}
	else {
		return reg.template acquire_write<element_t>(std::move(token), &mutex, held_locks);
	}
}

template <typename Access>
auto gse::access_trace_label() -> std::string {
	return format_access_label(is_read_access_v<Access> ? "read" : "write", type_tag<access_element_t<Access>>());
}

template <typename... Accesses>
auto gse::acquire_trace_id() -> id {
	static const id cached =
		make_acquire_trace_id(std::array<std::string, sizeof...(Accesses)>{ access_trace_label<Accesses>()... });
	return cached;
}

template <typename... Accesses>
auto gse::run_context::acquire() -> async::task<std::tuple<Accesses...>> {
	constexpr std::array<id, sizeof...(Accesses)> type_ids = { id_of<access_element_t<Accesses>>()... };
	constexpr std::array<lock_fn, sizeof...(Accesses)> fns = {
		(is_read_access_v<Accesses> ? &acquire_shared : &acquire_exclusive)...
	};
	static const id tid = acquire_trace_id<Accesses...>();
	co_await acquire_locks_in_sorted_order(m_access_mutexes, type_ids, fns, tid);
	co_return std::tuple<Accesses...>{
		make_locked_handle<Accesses>(
			access_token{},
			m_reg,
			m_access_mutexes,
			&m_held_locks
		)...
	};
}

template <typename... Tags>
auto gse::run_context::acquire_with(Tags...) -> async::task<std::tuple<tag_to_access_t<Tags>...>> {
	return acquire<tag_to_access_t<Tags>...>();
}

auto gse::run_context::held_lock_count() const -> int {
	return m_held_locks.load(std::memory_order_acquire);
}

auto gse::run_context::registry() const -> gse::registry& {
	return m_reg;
}

template <typename T>
auto gse::run_context::try_component(const id owner) const -> const T* {
	return m_reg.try_component<T>(owner);
}

template <typename T>
auto gse::run_context::components() const -> std::span<const T> {
	return m_reg.components<T>();
}

template <typename T>
auto gse::run_context::drain_component_adds() -> std::vector<id> {
	return m_reg.drain_component_adds<T>();
}

template <typename T>
auto gse::run_context::drain_component_updates() -> std::vector<id> {
	return m_reg.drain_component_updates<T>();
}

template <typename T>
auto gse::run_context::drain_component_removes() -> std::vector<id> {
	return m_reg.drain_component_removes<T>();
}

auto gse::run_context::ensure_exists(const id owner) -> void {
	m_reg.ensure_exists(owner);
}

auto gse::run_context::exists(const id owner) const -> bool {
	return m_reg.exists(owner);
}

auto gse::run_context::active(const id owner) const -> bool {
	return m_reg.active(owner);
}

auto gse::run_context::ensure_active(const id owner) -> void {
	m_reg.ensure_active(owner);
}

auto gse::run_context::remove(const id owner) -> void {
	m_reg.remove(owner);
}

template <typename T>
auto gse::run_context::add_component(const id owner, T value) -> T* {
	return m_reg.add_component<T>(owner, std::move(value));
}

template <typename T>
auto gse::run_context::remove_component(const id owner) -> void {
	m_reg.remove_component<T>(owner);
}

template <typename T>
auto gse::run_context::ensure_storage() -> void {
	m_reg.ensure_storage<T>();
}
