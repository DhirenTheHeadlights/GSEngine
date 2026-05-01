export module gse.ecs:registry;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;

import :access_token;
import :component;

export namespace gse {
	class component_storage_base {
	public:
		virtual ~component_storage_base(
		) = default;

		virtual auto activate_pending(
			id owner
		) -> bool = 0;

		virtual auto remove_owner(
			id owner
		) -> void = 0;
	};

	template <is_component T>
	class component_storage final : public component_storage_base {
	public:
		template <typename... Args>
		auto add(
			id owner,
			bool entity_active,
			Args&&... args
		) -> T*;

		auto activate_pending(
			id owner
		) -> bool override;

		auto remove_owner(
			id owner
		) -> void override;

		auto try_get(
			this component_storage& self,
			id owner
		) -> decltype(auto);

		auto items(
			this component_storage& self
		) -> decltype(auto);

		auto mark_updated(
			id owner
		) -> void;

		auto drain_added(
		) -> std::vector<id>;

		auto drain_updated(
		) -> std::vector<id>;

		auto drain_removed(
		) -> std::vector<id>;

	private:
		std::vector<T> m_data;
		std::vector<id> m_owners;
		std::unordered_map<id, std::uint32_t> m_index;

		std::unordered_map<id, T> m_pending;

		task::concurrent_queue<id> m_added_events;
		task::concurrent_queue<id> m_updated_events;
		task::concurrent_queue<id> m_removed_events;
	};

	class registry final : public non_copyable {
	public:
		~registry(
		) override = default;

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

		template <is_component T, typename... Args>
		auto add_component(
			id owner,
			Args&&... args
		) -> T*;

		template <is_component T>
		auto remove_component(
			id owner
		) -> void;

		template <is_component T>
		auto components(
			this registry& self
		) -> decltype(auto);

		template <is_component T>
		auto component(
			this registry& self,
			id owner
		) -> decltype(auto);

		template <is_component T>
		auto try_component(
			this registry& self,
			id owner
		) -> decltype(auto);

		template <is_component T>
		auto acquire_read(
			async::rw_mutex* mutex = nullptr
		) -> read<T>;

		template <is_component T>
		auto acquire_write(
			async::rw_mutex* mutex = nullptr
		) -> write<T>;

		template <is_component T>
		auto mark_component_updated(
			id owner
		) -> void;

		template <is_component T>
		auto drain_component_adds(
		) -> std::vector<id>;

		template <is_component T>
		auto drain_component_updates(
		) -> std::vector<id>;

		template <is_component T>
		auto drain_component_removes(
		) -> std::vector<id>;

	private:
		template <is_component T>
		auto storage(
		) -> component_storage<T>&;

		template <is_component T>
		auto try_storage(
			this registry& self
		) -> component_storage<T>*;

		std::unordered_set<id> m_active;
		std::unordered_set<id> m_inactive;

		std::unordered_map<id, std::unique_ptr<component_storage_base>> m_storages;
	};
}

template <gse::is_component T>
template <typename... Args>
auto gse::component_storage<T>::add(const id owner, const bool entity_active, Args&&... args) -> T* {
	if (entity_active) {
		if (const auto it = m_index.find(owner); it != m_index.end()) {
			return &m_data[it->second];
		}
		const auto idx = static_cast<std::uint32_t>(m_data.size());
		m_data.emplace_back(owner, std::forward<Args>(args)...);
		m_owners.push_back(owner);
		m_index.emplace(owner, idx);
		m_added_events.push(owner);
		return &m_data.back();
	}

	const auto [it, inserted] = m_pending.try_emplace(owner, owner, std::forward<Args>(args)...);
	return &it->second;
}

template <gse::is_component T>
auto gse::component_storage<T>::activate_pending(const id owner) -> bool {
	const auto it = m_pending.find(owner);
	if (it == m_pending.end()) {
		return false;
	}

	if (m_index.contains(owner)) {
		m_pending.erase(it);
		return false;
	}

	const auto idx = static_cast<std::uint32_t>(m_data.size());
	m_data.push_back(std::move(it->second));
	m_owners.push_back(owner);
	m_index.emplace(owner, idx);
	m_pending.erase(it);
	m_added_events.push(owner);
	return true;
}

template <gse::is_component T>
auto gse::component_storage<T>::remove_owner(const id owner) -> void {
	if (const auto pit = m_pending.find(owner); pit != m_pending.end()) {
		m_pending.erase(pit);
	}

	const auto it = m_index.find(owner);
	if (it == m_index.end()) {
		return;
	}

	const auto idx = it->second;
	const auto last = static_cast<std::uint32_t>(m_data.size() - 1);

	if (idx != last) {
		m_data[idx] = std::move(m_data[last]);
		m_owners[idx] = m_owners[last];
		m_index[m_owners[idx]] = idx;
	}

	m_data.pop_back();
	m_owners.pop_back();
	m_index.erase(it);
	m_removed_events.push(owner);
}

template <gse::is_component T>
auto gse::component_storage<T>::try_get(this component_storage& self, const id owner) -> decltype(auto) {
	const auto it = self.m_index.find(owner);
	return it == self.m_index.end() ? nullptr : &self.m_data[it->second];
}

template <gse::is_component T>
auto gse::component_storage<T>::items(this component_storage& self) -> decltype(auto) {
	return std::span(self.m_data);
}

template <gse::is_component T>
auto gse::component_storage<T>::mark_updated(const id owner) -> void {
	m_updated_events.push(owner);
}

template <gse::is_component T>
auto gse::component_storage<T>::drain_added() -> std::vector<id> {
	return m_added_events.drain();
}

template <gse::is_component T>
auto gse::component_storage<T>::drain_updated() -> std::vector<id> {
	return m_updated_events.drain();
}

template <gse::is_component T>
auto gse::component_storage<T>::drain_removed() -> std::vector<id> {
	return m_removed_events.drain();
}

auto gse::registry::create(const std::string_view name) -> id {
	const auto new_id = generate_id(name);
	m_inactive.insert(new_id);
	return new_id;
}

auto gse::registry::activate(const id owner) -> void {
	assert(
		m_inactive.contains(owner),
		std::source_location::current(),
		"Cannot activate entity with id {}: it is not inactive.",
		owner
	);

	m_inactive.erase(owner);
	m_active.insert(owner);

	for (const auto& s : std::views::values(m_storages)) {
		s->activate_pending(owner);
	}
}

auto gse::registry::remove(const id owner) -> void {
	m_active.erase(owner);
	m_inactive.erase(owner);

	for (const auto& s : std::views::values(m_storages)) {
		s->remove_owner(owner);
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

template <gse::is_component T>
auto gse::registry::storage() -> component_storage<T>& {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it != m_storages.end()) {
		return static_cast<component_storage<T>&>(*it->second);
	}

	auto fresh = std::make_unique<component_storage<T>>();
	auto& ref = *fresh;
	m_storages.emplace(type_idx, std::move(fresh));
	return ref;
}

template <gse::is_component T>
auto gse::registry::try_storage(this registry& self) -> component_storage<T>* {
	const auto type_idx = id_of<T>();
	const auto it = self.m_storages.find(type_idx);
	if (it == self.m_storages.end()) {
		return nullptr;
	}
	return static_cast<component_storage<T>*>(it->second.get());
}

template <gse::is_component T, typename... Args>
auto gse::registry::add_component(const id owner, Args&&... args) -> T* {
	assert(
		exists(owner),
		std::source_location::current(),
		"Cannot add component to entity with id {}: it does not exist.",
		owner
	);

	auto& s = storage<T>();
	return s.add(owner, active(owner), std::forward<Args>(args)...);
}

template <gse::is_component T>
auto gse::registry::remove_component(const id owner) -> void {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it == m_storages.end()) {
		return;
	}
	it->second->remove_owner(owner);
}

template <gse::is_component T>
auto gse::registry::components(this registry& self) -> decltype(auto) {
	auto* s = self.try_storage<T>();
	return s ? s->items() : std::span<T>{};
}

template <gse::is_component T>
auto gse::registry::component(this registry& self, const id owner) -> decltype(auto) {
	auto* ptr = self.try_component<T>(owner);
	assert(
		ptr != nullptr,
		std::source_location::current(),
		"Component of type {} with id {} not found.",
		type_tag<T>(),
		owner
	);
	return *ptr;
}

template <gse::is_component T>
auto gse::registry::try_component(this registry& self, const id owner) -> decltype(auto) {
	auto* s = self.try_storage<T>();
	return s ? s->try_get(owner) : nullptr;
}

template <gse::is_component T>
auto gse::registry::acquire_read(async::rw_mutex* mutex) -> read<T> {
	auto* s = try_storage<T>();
	if (!s) {
		return read<T>({}, nullptr, nullptr, mutex);
	}
	constexpr auto lookup = +[](void* ctx, const id owner) -> const T* {
		return static_cast<component_storage<T>*>(ctx)->try_get(owner);
	};
	return read<T>(s->items(), lookup, s, mutex);
}

template <gse::is_component T>
auto gse::registry::acquire_write(async::rw_mutex* mutex) -> write<T> {
	auto* s = try_storage<T>();
	if (!s) {
		return write<T>({}, nullptr, nullptr, mutex);
	}
	constexpr auto lookup = +[](void* ctx, const id owner) -> T* {
		return static_cast<component_storage<T>*>(ctx)->try_get(owner);
	};
	return write<T>(s->items(), lookup, s, mutex);
}

template <gse::is_component T>
auto gse::registry::mark_component_updated(const id owner) -> void {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it == m_storages.end()) {
		return;
	}
	static_cast<component_storage<T>&>(*it->second).mark_updated(owner);
}

template <gse::is_component T>
auto gse::registry::drain_component_adds() -> std::vector<id> {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it == m_storages.end()) {
		return {};
	}
	return static_cast<component_storage<T>&>(*it->second).drain_added();
}

template <gse::is_component T>
auto gse::registry::drain_component_updates() -> std::vector<id> {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it == m_storages.end()) {
		return {};
	}
	return static_cast<component_storage<T>&>(*it->second).drain_updated();
}

template <gse::is_component T>
auto gse::registry::drain_component_removes() -> std::vector<id> {
	const auto type_idx = id_of<T>();
	const auto it = m_storages.find(type_idx);
	if (it == m_storages.end()) {
		return {};
	}
	return static_cast<component_storage<T>&>(*it->second).drain_removed();
}

