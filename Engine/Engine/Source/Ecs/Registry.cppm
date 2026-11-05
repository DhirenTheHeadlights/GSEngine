export module gse.ecs:registry;

import gse.assert;
import gse.core;
import std;

import :access;
import :component;
import :component_storage;
import :traits;

export namespace gse {
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

		auto has_components(
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
		auto mark_component_updated(
			id owner
		) -> void;

		template <typename T>
		auto ensure_storage() -> void;

	private:
		friend class context;

		template <typename Access>
		auto acquire(
			std::atomic<int>& held_locks
		) -> Access;

		struct storage_slot {
			std::unique_ptr<void, void (*)(void*)> storage{ nullptr, nullptr };
			bool (*activate_pending)(void*, id) = nullptr;
			void (*remove_owner)(void*, id) = nullptr;
			bool (*contains)(void*, id) = nullptr;
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

auto gse::registry::has_components(const id owner) const -> bool {
	for (const auto& slot : std::views::values(m_storages)) {
		if (slot.storage && slot.contains(slot.storage.get(), owner)) {
			return true;
		}
	}
	return false;
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
		slot.contains = +[](void* ptr, const id owner) {
			return static_cast<component_storage<T>*>(ptr)->try_get(owner) != nullptr;
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

template <typename Access>
auto gse::registry::acquire(std::atomic<int>& held_locks) -> Access {
	return Access(*try_storage<access_element_t<Access>>(), held_locks);
}

template <typename T>
auto gse::registry::mark_component_updated(const id owner) -> void {
	if (auto* s = try_storage<T>()) {
		s->mark_updated(owner);
	}
}