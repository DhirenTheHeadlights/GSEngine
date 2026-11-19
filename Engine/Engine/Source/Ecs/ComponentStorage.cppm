export module gse.ecs:component_storage;

import gse.assert;
import gse.concurrency;
import gse.core;
import std;

export namespace gse {
	class access_counters {
	public:
		auto begin_read(
			id type,
			const void* owner
		) -> void;

		auto end_read() -> void;

		auto begin_write(
			id type,
			const void* owner
		) -> void;

		auto end_write() -> void;

	private:
		std::atomic<int> m_readers{ 0 };
		std::atomic<int> m_writers{ 0 };
		std::atomic<const void*> m_writer_owner{ nullptr };
	};

	template <typename T>
	class component_storage {
	public:
		auto add(
			id owner,
			bool entity_active,
			T value = T{}
		) -> T*;

		auto activate_pending(
			id owner
		) -> bool;

		auto remove_owner(
			id owner
		) -> void;

		auto try_get(
			id owner
		) -> T*;

		auto items() -> std::span<T>;

		auto owners() const -> std::span<const id>;

		auto mark_updated(
			id owner
		) -> void;

		auto drain_added() -> std::vector<id>;

		auto drain_updated() -> std::vector<id>;

		auto drain_removed() -> std::vector<id>;

		auto counters() -> access_counters&;

	private:
		id_mapped_collection<T> m_items;
		std::unordered_map<id, T> m_pending;

		task::concurrent_queue<id> m_added_events;
		task::concurrent_queue<id> m_updated_events;
		task::concurrent_queue<id> m_removed_events;

		access_counters m_counters;
	};
}

auto gse::access_counters::begin_read(const id type, const void* owner) -> void {
	m_readers.fetch_add(1, std::memory_order_acq_rel);
	const bool writer_is_self = m_writer_owner.load(std::memory_order_acquire) == owner;
	assert(m_writers.load(std::memory_order_acquire) == 0 || writer_is_self, "data race: read access on component {} while another system is writing it (missing scheduler dependency)", type);
}

auto gse::access_counters::end_read() -> void {
	m_readers.fetch_sub(1, std::memory_order_acq_rel);
}

auto gse::access_counters::begin_write(const id type, const void* owner) -> void {
	const int prev_writers = m_writers.fetch_add(1, std::memory_order_acq_rel);
	const int readers = m_readers.load(std::memory_order_acquire);

	if (prev_writers == 0) {
		m_writer_owner.store(owner, std::memory_order_release);
		assert(readers == 0, "data race: exclusive access on component {} while another system is reading it (missing scheduler dependency)", type);
		return;
	}

	assert(m_writer_owner.load(std::memory_order_acquire) == owner, "data race: exclusive access on component {} while another system is already accessing it (missing scheduler dependency)", type);
}

auto gse::access_counters::end_write() -> void {
	if (m_writers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		m_writer_owner.store(nullptr, std::memory_order_release);
	}
}

template <typename T>
auto gse::component_storage<T>::add(const id owner, const bool entity_active, T value) -> T* {
	if (!entity_active) {
		const auto [it, inserted] = m_pending.try_emplace(owner, std::move(value));
		return &it->second;
	}

	if (auto* existing = m_items.try_get(owner)) {
		return existing;
	}

	auto* added = m_items.add(owner, std::move(value));
	m_added_events.push(owner);
	return added;
}

template <typename T>
auto gse::component_storage<T>::activate_pending(const id owner) -> bool {
	const auto it = m_pending.find(owner);
	if (it == m_pending.end()) {
		return false;
	}

	if (m_items.contains(owner)) {
		m_pending.erase(it);
		return false;
	}

	m_items.add(owner, std::move(it->second));
	m_pending.erase(it);
	m_added_events.push(owner);
	return true;
}

template <typename T>
auto gse::component_storage<T>::remove_owner(const id owner) -> void {
	if (const auto pit = m_pending.find(owner); pit != m_pending.end()) {
		m_pending.erase(pit);
	}

	if (!m_items.contains(owner)) {
		return;
	}

	m_items.remove(owner);
	m_removed_events.push(owner);
}

template <typename T>
auto gse::component_storage<T>::try_get(const id owner) -> T* {
	return m_items.try_get(owner);
}

template <typename T>
auto gse::component_storage<T>::items() -> std::span<T> {
	return m_items.items();
}

template <typename T>
auto gse::component_storage<T>::owners() const -> std::span<const id> {
	return m_items.ids();
}

template <typename T>
auto gse::component_storage<T>::mark_updated(const id owner) -> void {
	m_updated_events.push(owner);
}

template <typename T>
auto gse::component_storage<T>::drain_added() -> std::vector<id> {
	return m_added_events.drain();
}

template <typename T>
auto gse::component_storage<T>::drain_updated() -> std::vector<id> {
	return m_updated_events.drain();
}

template <typename T>
auto gse::component_storage<T>::drain_removed() -> std::vector<id> {
	return m_removed_events.drain();
}

template <typename T>
auto gse::component_storage<T>::counters() -> access_counters& {
	return m_counters;
}
