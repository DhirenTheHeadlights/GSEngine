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

	template <typename... Ts>
	struct reads {
		static auto component_ids() -> std::array<id, sizeof...(Ts)>;
	};

	template <typename... Ts>
	struct writes {
		static auto component_ids() -> std::array<id, sizeof...(Ts)>;
	};

	template <typename T>
	constexpr bool is_reads_v = false;

	template <typename... Ts>
	constexpr bool is_reads_v<reads<Ts...>> = true;

	template <typename T>
	constexpr bool is_writes_v = false;

	template <typename... Ts>
	constexpr bool is_writes_v<writes<Ts...>> = true;

	template <typename T>
	constexpr bool is_access_decl_v = is_reads_v<T> || is_writes_v<T>;

	struct access_lint {
		std::uint32_t runs = 0;
		std::uint32_t acquire_runs = 0;
		std::uint32_t acquires_this_run = 0;
		bool structural_this_run = false;
		bool multi_acquire_seen = false;
		bool structural_seen = false;
		bool warned = false;
	};

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
			async::manual_event& paused_event
		);

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> void;

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

		[[nodiscard]] auto access_mutexes() const -> async::rw_mutex_registry&;

		template <typename Access>
		auto make_access() -> Access;

		template <typename T>
		auto make_structural() -> structural<T>;

		auto set_access_lint(
			access_lint* lint
		) -> void;

	private:
		scheduler& m_sched;
		gse::registry& m_reg;
		async::rw_mutex_registry& m_access_mutexes;
		std::atomic<int> m_held_locks{ 0 };
		access_lint* m_lint = nullptr;
		async::manual_event& m_resume_event;
		async::manual_event& m_paused_event;
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

	template <typename S, typename... Args>
	auto queue_add_system_helper(
		scheduler& sched,
		Args&&... args
	) -> void;
}

gse::run_context::run_context(scheduler& sched, state_registry& states, resource_registry& resources_store, channel_registry& channels_store, channel_writer& channels, task_graph& graph, gse::registry& reg, async::rw_mutex_registry& access_mutexes, async::manual_event& resume_event, async::manual_event& paused_event)
	: task_context{ states, resources_store, channels_store, channels, graph, true }, m_sched(sched), m_reg(reg), m_access_mutexes(access_mutexes), m_resume_event(resume_event), m_paused_event(paused_event) {
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
auto gse::run_context::make_access() -> Access {
	return make_locked_handle<Access>(access_token{}, m_reg, m_access_mutexes, &m_held_locks);
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
	if (m_lint) {
		++m_lint->acquires_this_run;
	}
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

template <typename... Ts>
auto gse::reads<Ts...>::component_ids() -> std::array<id, sizeof...(Ts)> {
	return { id_of<Ts>()... };
}

template <typename... Ts>
auto gse::writes<Ts...>::component_ids() -> std::array<id, sizeof...(Ts)> {
	return { id_of<Ts>()... };
}

auto gse::run_context::held_lock_count() const -> int {
	return m_held_locks.load(std::memory_order_acquire);
}

auto gse::run_context::set_access_lint(access_lint* lint) -> void {
	m_lint = lint;
}

auto gse::run_context::access_mutexes() const -> async::rw_mutex_registry& {
	return m_access_mutexes;
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
	if (m_lint) {
		m_lint->structural_this_run = true;
	}
	return m_reg.add_component<T>(owner, std::move(value));
}

template <typename T>
auto gse::run_context::remove_component(const id owner) -> void {
	if (m_lint) {
		m_lint->structural_this_run = true;
	}
	m_reg.remove_component<T>(owner);
}

template <typename T>
auto gse::run_context::ensure_storage() -> void {
	if (m_lint) {
		m_lint->structural_this_run = true;
	}
	m_reg.ensure_storage<T>();
}

template <typename S, typename... Args>
auto gse::run_context::add_system(Args&&... args) -> void {
	queue_add_system_helper<S>(m_sched, std::forward<Args>(args)...);
}

template <typename T>
auto gse::run_context::make_structural() -> structural<T> {
	auto& mutex = m_access_mutexes.mutex_for(id_of<T>());
	return structural<T>(&m_reg, &mutex, &m_held_locks);
}

template <typename T>
auto gse::structural<T>::add(const id owner, T value) const -> T* {
	return m_reg->add_component<T>(owner, std::move(value));
}

template <typename T>
auto gse::structural<T>::remove(const id owner) const -> void {
	m_reg->remove_component<T>(owner);
}

template <typename T>
auto gse::structural<T>::ensure_storage() const -> void {
	m_reg->ensure_storage<T>();
}
