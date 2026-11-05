export module gse.ecs:access;

import gse.core;
import std;

import :component_storage;

export namespace gse {
	enum class access_mode : std::uint8_t {
		read,
		write
	};

	class registry;
	class context;

	enum class component_event : std::uint8_t {
		added,
		updated,
		removed
	};

	template <typename T>
	struct component_events {
		std::vector<id> added;
		std::vector<id> updated;
		std::vector<id> removed;
	};

	class component_lock : non_copyable {
	public:
		struct config {
			access_counters* counters = nullptr;
			std::atomic<int>* held_locks = nullptr;
			id type;
			access_mode mode = access_mode::read;
		};

		~component_lock();

		component_lock() = default;

		explicit component_lock(
			config cfg
		);

		component_lock(
			component_lock&& other
		) noexcept;

		auto operator=(
			component_lock&& other
		) noexcept -> component_lock&;

	private:
		auto release() -> void;

		config m_config;
	};

	template <typename T, access_mode M = access_mode::read>
	class access : non_copyable {
	public:
		using value_type = std::conditional_t<M == access_mode::read, const T, T>;
		using pointer = value_type*;
		using reference = value_type&;

		~access() = default;

		access() = delete;

		access(
			access&& other
		) noexcept = default;

		auto operator=(
			access&& other
		) noexcept -> access& = default;

		[[nodiscard]] auto begin() const -> pointer;

		[[nodiscard]] auto end() const -> pointer;

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto data() const -> pointer;

		auto operator[](
			std::size_t i
		) const -> reference;

		auto find(
			id owner
		) const -> pointer;

		[[nodiscard]] auto owner_ids() const -> std::span<const id>;

		[[nodiscard]] auto owner_id_at(
			std::size_t i
		) const -> id;

		auto mark_updated(
			id owner
		) const -> void;

		auto drain(
			component_event event
		) const -> std::vector<id>;

	private:
		friend class registry;

		access(
			component_storage<T>& storage,
			std::atomic<int>& held_locks
		);

		component_storage<T>* m_storage;
		component_lock m_lock;
	};

	template <typename T>
	using read = access<T, access_mode::read>;

	template <typename T>
	using write = access<T, access_mode::write>;

	class entities : non_copyable {
	public:
		~entities() = default;

		entities(
			entities&& other
		) noexcept = default;

		auto operator=(
			entities&& other
		) noexcept -> entities& = default;

		auto ensure_exists(
			id owner
		) const -> void;

		[[nodiscard]] auto exists(
			id owner
		) const -> bool;

		[[nodiscard]] auto active(
			id owner
		) const -> bool;

		[[nodiscard]] auto has_components(
			id owner
		) const -> bool;

		auto ensure_active(
			id owner
		) const -> void;

		auto remove(
			id owner
		) const -> void;

		template <typename T>
		auto add_component(
			id owner,
			T value = T{}
		) const -> T*;

		template <typename T>
		auto remove_component(
			id owner
		) const -> void;

		template <typename T>
		auto ensure_storage() const -> void;

	private:
		friend class context;

		explicit entities(
			registry* reg
		);

		registry* m_reg = nullptr;
	};
}

gse::component_lock::component_lock(const config cfg) : m_config(cfg) {
	if (m_config.mode == access_mode::read) {
		m_config.counters->begin_read(m_config.type, m_config.held_locks);
	}
	else {
		m_config.counters->begin_write(m_config.type, m_config.held_locks);
	}
	m_config.held_locks->fetch_add(1, std::memory_order_acq_rel);
}

gse::component_lock::component_lock(component_lock&& other) noexcept : m_config(std::exchange(other.m_config, config{})) {}

auto gse::component_lock::operator=(component_lock&& other) noexcept -> component_lock& {
	if (this != &other) {
		release();
		m_config = std::exchange(other.m_config, config{});
	}
	return *this;
}

gse::component_lock::~component_lock() {
	release();
}

auto gse::component_lock::release() -> void {
	if (!m_config.counters) {
		return;
	}
	if (m_config.mode == access_mode::read) {
		m_config.counters->end_read();
	}
	else {
		m_config.counters->end_write();
	}
	m_config.held_locks->fetch_sub(1, std::memory_order_acq_rel);
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(component_storage<T>& storage, std::atomic<int>& held_locks) : m_storage(&storage) {
	m_lock = component_lock({
		.counters = &storage.counters(),
		.held_locks = &held_locks,
		.type = id_of<T>(),
		.mode = M,
	});
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::begin() const -> pointer {
	return m_storage->items().data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::end() const -> pointer {
	return begin() + size();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::size() const -> std::size_t {
	return m_storage->items().size();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::empty() const -> bool {
	return m_storage->items().empty();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::data() const -> pointer {
	return m_storage->items().data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) const -> reference {
	return m_storage->items()[i];
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::find(const id owner) const -> pointer {
	return m_storage->try_get(owner);
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::owner_ids() const -> std::span<const id> {
	return m_storage->owners();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::owner_id_at(const std::size_t i) const -> id {
	return m_storage->owners()[i];
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::mark_updated(const id owner) const -> void {
	m_storage->mark_updated(owner);
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::drain(const component_event event) const -> std::vector<id> {
	static_assert(
		M == access_mode::write,
		"drain() consumes the component event queue, so it requires write<T>; systems holding read<T> are not ordered against each other and would each receive an arbitrary subset of the events"
	);
	switch (event) {
		case component_event::added:
			return m_storage->drain_added();
		case component_event::updated:
			return m_storage->drain_updated();
		case component_event::removed:
			return m_storage->drain_removed();
	}
	return {};
}
