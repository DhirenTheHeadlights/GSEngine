export module gse.ecs:registry;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;

import :access_token;
import :component;

export namespace gse {
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

	private:
		id_mapped_collection<T> m_items;
		std::unordered_map<id, T> m_pending;

		task::concurrent_queue<id> m_added_events;
		task::concurrent_queue<id> m_updated_events;
		task::concurrent_queue<id> m_removed_events;
	};

	class registry final : public non_copyable {
	public:
		auto create(
			std::string_view name
		) -> id;

		auto activate(
			id owner
		) -> void;

		auto remove(
			id owner
		) -> void;

		auto ensure_exists(
			id owner
		) -> void;

		auto ensure_active(
			id owner
		) -> void;

		auto exists(
			id owner
		) const -> bool;

		auto active(
			id owner
		) const -> bool;

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
		auto components(
			this registry& self
		) -> decltype(auto);

		template <typename T>
		auto owner_ids(
			this registry& self
		) -> std::span<const id>;

		template <typename T>
		auto component(
			this registry& self,
			id owner
		) -> decltype(auto);

		template <typename T>
		auto try_component(
			this registry& self,
			id owner
		) -> decltype(auto);

		template <typename T>
		auto acquire_read(
			access_token,
			access_guard* guard = nullptr,
			std::atomic<int>* held_locks = nullptr
		) -> read<T>;

		template <typename T>
		auto acquire_write(
			access_token,
			access_guard* guard = nullptr,
			std::atomic<int>* held_locks = nullptr
		) -> write<T>;

		template <typename T>
		auto mark_component_updated(
			id owner
		) -> void;

		template <typename T>
		auto ensure_storage() -> void;

	private:
		struct storage_slot {
			std::unique_ptr<void, void (*)(void*)> storage{ nullptr, nullptr };
			bool (*activate_pending)(void*, id) = nullptr;
			void (*remove_owner)(void*, id) = nullptr;
		};

		template <typename T>
		auto storage() -> component_storage<T>&;

		template <typename T>
		auto try_storage(
			this registry& self
		) -> component_storage<T>*;

		std::unordered_set<id> m_active;
		std::unordered_set<id> m_inactive;

		std::unordered_map<id, storage_slot> m_storages;
	};
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

auto gse::registry::create(const std::string_view name) -> id {
	const auto new_id = generate_id(name);
	m_inactive.insert(new_id);
	return new_id;
}

auto gse::registry::activate(const id owner) -> void {
	assert(m_inactive.contains(owner), "Cannot activate entity with id {}: it is not inactive.", owner);

	m_inactive.erase(owner);
	m_active.insert(owner);

	for (auto& slot : std::views::values(m_storages)) {
		if (slot.storage) {
			slot.activate_pending(slot.storage.get(), owner);
		}
	}
}

auto gse::registry::remove(const id owner) -> void {
	m_active.erase(owner);
	m_inactive.erase(owner);

	for (auto& slot : std::views::values(m_storages)) {
		if (slot.storage) {
			slot.remove_owner(slot.storage.get(), owner);
		}
	}
}

auto gse::registry::ensure_exists(const id owner) -> void {
	if (exists(owner)) {
		return;
	}
	m_inactive.insert(owner);
}

auto gse::registry::ensure_active(const id owner) -> void {
	if (!exists(owner)) {
		m_inactive.insert(owner);
	}
	if (!active(owner)) {
		activate(owner);
	}
}

auto gse::registry::exists(const id owner) const -> bool {
	return m_active.contains(owner) || m_inactive.contains(owner);
}

auto gse::registry::active(const id owner) const -> bool {
	return m_active.contains(owner);
}

template <typename T>
auto gse::registry::storage() -> component_storage<T>& {
	const auto type_idx = id_of<T>();
	auto& slot = m_storages[type_idx];
	if (!slot.storage) {
		slot.storage = std::unique_ptr<void, void (*)(void*)>(
			new component_storage<T>(),
			+[](void* ptr) {
				delete static_cast<component_storage<T>*>(ptr);
			}
		);
		slot.activate_pending = +[](void* ptr, const id owner) {
			return static_cast<component_storage<T>*>(ptr)->activate_pending(owner);
		};
		slot.remove_owner = +[](void* ptr, const id owner) {
			static_cast<component_storage<T>*>(ptr)->remove_owner(owner);
		};
	}

	return *static_cast<component_storage<T>*>(slot.storage.get());
}

template <typename T>
auto gse::registry::ensure_storage() -> void {
	(void)storage<T>();
}

template <typename T>
auto gse::registry::try_storage(this registry& self) -> component_storage<T>* {
	const auto type_idx = id_of<T>();
	const auto it = self.m_storages.find(type_idx);
	if (it == self.m_storages.end()) {
		return nullptr;
	}
	return static_cast<component_storage<T>*>(it->second.storage.get());
}

template <typename T>
auto gse::registry::add_component(const id owner, T value) -> T* {
	assert(exists(owner), "Cannot add component to entity with id {}: it does not exist.", owner);

	auto& s = storage<T>();
	return s.add(owner, active(owner), std::move(value));
}

template <typename T>
auto gse::registry::remove_component(const id owner) -> void {
	if (auto* s = try_storage<T>()) {
		s->remove_owner(owner);
	}
}

template <typename T>
auto gse::registry::components(this registry& self) -> decltype(auto) {
	auto* s = self.try_storage<T>();
	return s ? s->items() : std::span<T>{};
}

template <typename T>
auto gse::registry::owner_ids(this registry& self) -> std::span<const id> {
	auto* s = self.try_storage<T>();
	return s ? s->owners() : std::span<const id>{};
}

template <typename T>
auto gse::registry::component(this registry& self, const id owner) -> decltype(auto) {
	auto* ptr = self.try_component<T>(owner);
	assert(ptr != nullptr, "Component of type {} with id {} not found.", type_tag<T>(), owner);
	return *ptr;
}

template <typename T>
auto gse::registry::try_component(this registry& self, const id owner) -> decltype(auto) {
	auto* s = self.try_storage<T>();
	return s ? s->try_get(owner) : nullptr;
}

template <typename T>
auto gse::registry::acquire_read(access_token, access_guard* guard, std::atomic<int>* held_locks) -> read<T> {
	auto* s = try_storage<T>();
	if (!s) {
		return read<T>({}, {}, {
			.guard = guard,
			.held_locks = held_locks,
		});
	}
	constexpr auto lookup = +[](void* ctx, const id owner) -> const T* {
		return static_cast<component_storage<T>*>(ctx)->try_get(owner);
	};
	constexpr auto mark = +[](void* ctx, const id owner) -> void {
		static_cast<component_storage<T>*>(ctx)->mark_updated(owner);
	};
	return read<T>(s->items(), s->owners(), {
		.lookup = lookup,
		.mark = mark,
		.ctx = s,
		.guard = guard,
		.held_locks = held_locks,
	});
}

template <typename T>
auto gse::registry::acquire_write(access_token, access_guard* guard, std::atomic<int>* held_locks) -> write<T> {
	auto* s = try_storage<T>();
	if (!s) {
		return write<T>({}, {}, {
			.guard = guard,
			.held_locks = held_locks,
		});
	}
	constexpr auto lookup = +[](void* ctx, const id owner) -> T* {
		return static_cast<component_storage<T>*>(ctx)->try_get(owner);
	};
	constexpr auto mark = +[](void* ctx, const id owner) -> void {
		static_cast<component_storage<T>*>(ctx)->mark_updated(owner);
	};
	constexpr auto drain = +[](void* ctx, const component_event event) -> std::vector<id> {
		auto* storage = static_cast<component_storage<T>*>(ctx);
		switch (event) {
			case component_event::added:
				return storage->drain_added();
			case component_event::updated:
				return storage->drain_updated();
			case component_event::removed:
				return storage->drain_removed();
		}
		return {};
	};
	return write<T>(s->items(), s->owners(), {
		.lookup = lookup,
		.mark = mark,
		.drain = drain,
		.ctx = s,
		.guard = guard,
		.held_locks = held_locks,
	});
}

template <typename T>
auto gse::registry::mark_component_updated(const id owner) -> void {
	if (auto* s = try_storage<T>()) {
		s->mark_updated(owner);
	}
}