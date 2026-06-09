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
import :task_graph;
import :traits;

export namespace gse {
	class scheduler;

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
			access_guard& guard,
			async::manual_event& resume_event,
			async::manual_event& paused_event
		);

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> void;

		[[nodiscard]] auto yield_tick() -> async::task<>;

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

		[[nodiscard]] auto has_structural_authority(
			id type
		) const -> bool;

		template <typename Access>
		auto make_access() -> Access;

		template <typename T>
		auto make_structural() -> structural<T>;

	private:
		scheduler& m_sched;
		gse::registry& m_reg;
		access_guard& m_guard;
		std::atomic<int> m_held_locks{ 0 };
		std::vector<id> m_structural_authority;
		async::manual_event& m_resume_event;
		async::manual_event& m_paused_event;
	};
}

namespace gse {
	template <typename Access>
	auto make_locked_handle(
		access_token token,
		registry& reg,
		access_guard& guard,
		std::atomic<int>* held_locks
	) -> Access;

	template <typename S, typename... Args>
	auto queue_add_system_helper(
		scheduler& sched,
		Args&&... args
	) -> void;
}

gse::run_context::run_context(scheduler& sched, state_registry& states, resource_registry& resources_store, channel_registry& channels_store, channel_writer& channels, task_graph& graph, gse::registry& reg, access_guard& guard, async::manual_event& resume_event, async::manual_event& paused_event)
	: task_context{ states, resources_store, channels_store, channels, graph, true }, m_sched(sched), m_reg(reg), m_guard(guard), m_resume_event(resume_event), m_paused_event(paused_event) {
}


template <typename Access>
auto gse::make_locked_handle(access_token token, registry& reg, access_guard& guard, std::atomic<int>* held_locks) -> Access {
	using element_t = access_element_t<Access>;
	if constexpr (is_read_access_v<Access>) {
		return reg.template acquire_read<element_t>(std::move(token), &guard, held_locks);
	}
	else {
		return reg.template acquire_write<element_t>(std::move(token), &guard, held_locks);
	}
}

template <typename Access>
auto gse::run_context::make_access() -> Access {
	return make_locked_handle<Access>(access_token{}, m_reg, m_guard, &m_held_locks);
}

auto gse::run_context::held_lock_count() const -> int {
	return m_held_locks.load(std::memory_order_acquire);
}

auto gse::run_context::has_structural_authority(const id type) const -> bool {
	return std::ranges::find(m_structural_authority, type) != m_structural_authority.end();
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
	assert(has_structural_authority(id_of<T>()), "add_component<{}> requires structural<{}> authority in the current phase", type_tag<T>(), type_tag<T>());
	return m_reg.add_component<T>(owner, std::move(value));
}

template <typename T>
auto gse::run_context::remove_component(const id owner) -> void {
	assert(has_structural_authority(id_of<T>()), "remove_component<{}> requires structural<{}> authority in the current phase", type_tag<T>(), type_tag<T>());
	m_reg.remove_component<T>(owner);
}

template <typename T>
auto gse::run_context::ensure_storage() -> void {
	assert(has_structural_authority(id_of<T>()), "ensure_storage<{}> requires structural<{}> authority in the current phase", type_tag<T>(), type_tag<T>());
	m_reg.ensure_storage<T>();
}

template <typename S, typename... Args>
auto gse::run_context::add_system(Args&&... args) -> void {
	queue_add_system_helper<S>(m_sched, std::forward<Args>(args)...);
}

template <typename T>
auto gse::run_context::make_structural() -> structural<T> {
	return structural<T>(&m_reg, &m_guard, &m_held_locks, &m_structural_authority);
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
